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

#include <math.h>

/* utility */
#include "log.h"
#include "rand.h"

/* common */
#include "city.h"
#include "effects.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "specialist.h"
#include "traits.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* ai */
#include "aitraits.h"
#include "difficulty.h"
#include "handicaps.h"

/* server */
#include "cityhand.h"
#include "maphand.h"
#include "citytools.h"
#include "cityturn.h"
#include "sernet.h"
#include "srv_log.h"
#include "unithand.h"

/* server/advisors */
#include "advbuilding.h"
#include "advchoice.h"
#include "advdata.h"
#include "advgoto.h"
#include "infracache.h"

/* ai/default */
#include "daiair.h"
#include "daicity.h"
#include "daidata.h"
#include "daidiplomacy.h"
#include "daidiplomat.h"
#include "daidomestic.h"
#include "daiguard.h"
#include "daieffects.h"
#include "daiferry.h"
#include "daihand.h"
#include "daihunter.h"
#include "daimilitary.h"
#include "daiparadrop.h"
#include "daiplayer.h"
#include "daisettler.h"
#include "daitech.h"
#include "daitools.h"
#include "daiunit.h"

#include "deepai.h"
#include "deepcombat.h"
#include "deepexplore.h"
#include "deephorizon.h"
#include "deepsite.h"
#include "deepcm.h"
#include "deepops.h"
#include "deeprates.h"
#include "deepstance.h"
#include "deeptrade.h"
#include "deeptuning.h"
#include "deepwant.h"

#include "deepdefence.h"

#define LOGLEVEL_BUILD LOG_DEBUG
#define LOG_BUY LOG_DEBUG

/* Defence bookkeeping (W11). Stock cities only count defenders that are
 * physically on the city tile (assess_defense_backend), so a city whose
 * help is two turns out keeps requisitioning at emergency priority; and
 * simultaneous city emergencies are resolved in city list order with no
 * arbitration. The deep turn drivers below are derived from
 * dai_do_first_activities() / dai_do_last_activities() /
 * dai_manage_cities() so those two behaviours can change; everything
 * else is the stock call sequence, reused. */

/**********************************************************************//**
  Log one bookkeeping line per city: the raw assessed danger, the
  on-tile defence, and what the city is building
  (benchmark/defence.py). Runs before any deep adjustment so before and
  after arms measure need on the same scale.
**************************************************************************/
static double deep_garrison_menace(struct player *pplayer);
static void deep_assault_want(struct ai_type *ait,
                              const struct civ_map *nmap,
                              struct player *pplayer, struct city *pcity,
                              struct unit_type *punittype,
                              struct adv_choice *choice);
static bool deep_boat_choice_refused(struct ai_type *ait,
                                     const struct adv_choice *choice,
                                     const struct city *pcity);

static void deep_log_defence(struct ai_type *ait, struct player *pplayer)
{
  double menace = deep_garrison_menace(pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    int ndef = 0;

    unit_list_iterate(city_tile(pcity)->units, punit) {
      if (unit_owner(punit) == pplayer
          && !unit_has_type_flag(punit, UTYF_CIVILIAN)) {
        ndef++;
      }
    } unit_list_iterate_end;

    log_base(deep_metric_log_level(),
             "deep-def turn=%d player=\"%s\" city=%d danger=%u"
             " urgency=%u defense=%d ndef=%d prod=\"%s\""
             " shield=%d food=%d menace=%.2f"
             " founder=%d worker=%d",
             game.info.turn, player_name(pplayer), pcity->id,
             city_data->danger, city_data->urgency,
             assess_defense(ait, pcity), ndef,
             universal_rule_name(&pcity->production),
             pcity->surplus[O_SHIELD], pcity->surplus[O_FOOD],
             menace,
             /* Expansion's side of the scale. `danger` above is the
              * RAW stock figure -- deep's injected fears land after
              * this line -- so pairing it with founder_want is the
              * only way to see what settlers were actually weighed
              * against. Both wants are cached across
              * AI_CITY_RECALC_SPEED turns, which is what the build
              * choice reads too.
              *
              * city_data->choice is deliberately NOT logged here: this
              * pass runs before the choice is made, so it still holds
              * want=-1 and would read as "expansion lost to nothing". */
             city_data->founder_want, city_data->worker_want);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Credit defenders that are on their way home against their target
  city's freshly assessed danger, so the city stops requisitioning
  replacements for help that is already coming. This is the missing
  mirror of the invasion bookkeeping the AI keeps about enemy cities
  (reinforcements_cost_and_value() filling ai_invasion).

  A unit counts if its task is AIUNIT_DEFEND_HOME and its goto points
  at an own city it has not reached yet; its contribution is its
  defence power in that city discounted by expected arrival time and
  the chance of surviving the trip.
**************************************************************************/
static unsigned int deep_incoming_defence_credit(struct ai_type *ait,
                                                 struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  unsigned int credit = 0;

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *unit_data = def_ai_unit_data(punit, ait);
    int eta, power;

    bool coming;

    /* Two ways help is already on its way: a defender sent home by
     * stock, and a blade the counter-claim summoned (ESCORT with
     * this city as its guard charge). Both must count, or the city
     * requisitions a replacement for relief it has already been
     * promised -- and buying a defender it does not need is a
     * granary it never builds. */
    coming = (unit_data->task == AIUNIT_DEFEND_HOME
              && punit->goto_tile != NULL
              && tile_city(punit->goto_tile) == pcity)
             || (unit_data->task == AIUNIT_ESCORT
                 && aiguard_charge_city(ait, punit) == pcity);
    if (!coming || same_pos(unit_tile(punit), city_tile(pcity))) {
      /* Units already in place are counted by assess_defense(). */
      continue;
    }

    power = assess_defense_unit(ait, pcity, punit, FALSE);
    if (power <= 0) {
      continue;
    }
    eta = real_map_distance(unit_tile(punit), city_tile(pcity))
          * SINGLE_MOVE / MAX(unit_type_get(punit)->move_rate, 1);
    credit += power
              * pow(tune->incoming_defence_survival, eta)
              / (1.0 + tune->incoming_defence_eta_weight * eta);
  } unit_list_iterate_end;

  return credit;
}

/**********************************************************************//**
  A mustering army is a reserve (human doctrine 2026-08-01: attack
  plans belong inside defence plans). Members of an assault op still
  in MUSTER have not been spent -- the counter-claim and the poach
  can redeploy them -- so each credits nearby cities' danger exactly
  like an en-route defender: same formula, same knobs (W11), decay
  doing the "nearby". Launched members (MARCH onward) credit
  nothing: committed is committed.
**************************************************************************/
static unsigned int deep_muster_reserve_credit(struct ai_type *ait,
                                               struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  const struct deep_plr *dp = deep_plr_data(pplayer);
  unsigned int credit = 0;
  int oi, i;

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    const struct deep_operation *op = &dp->ops[oi];

    if (op->state != DEEP_OP_MUSTER || op->kind != DEEP_OPK_ASSAULT) {
      continue;
    }
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);
      int eta, power;

      if (m == NULL || m->hp <= 0
          || same_pos(unit_tile(m), city_tile(pcity))
          || tile_continent(unit_tile(m))
             != tile_continent(city_tile(pcity))) {
        /* Units standing here are already counted by
         * assess_defense(). */
        continue;
      }
      power = assess_defense_unit(ait, pcity, m, FALSE);
      if (power <= 0) {
        continue;
      }
      eta = real_map_distance(unit_tile(m), city_tile(pcity))
            * SINGLE_MOVE / MAX(unit_type_get(m)->move_rate, 1);
      credit += power
                * pow(tune->incoming_defence_survival, eta)
                / (1.0 + tune->incoming_defence_eta_weight * eta);
    }
  }

  return credit;
}

/**********************************************************************//**
  Is every committed blade too far to matter here? TRUE when the
  player has at least one LAUNCHED assault (MARCH or beyond) and
  none of its members could reach this city within the survival
  horizon -- the army is away, the home front is thinner than the
  unit count suggests, and the fear conversion scales up by
  committed_fear_mult (human doctrine 2026-08-01).
**************************************************************************/
static bool deep_committed_away(struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct deep_plr *dp = deep_plr_data(city_owner(pcity));
  bool any = FALSE;
  int oi, i;

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    const struct deep_operation *op = &dp->ops[oi];

    if (op->kind != DEEP_OPK_ASSAULT
        || (op->state != DEEP_OP_MARCH && op->state != DEEP_OP_STRIKE
            && op->state != DEEP_OP_CONSOLIDATE)) {
      continue;
    }
    for (i = 0; i < op->n_members; i++) {
      const struct unit *m = game_unit_by_number(op->members[i]);
      int eta;

      if (m == NULL || m->hp <= 0) {
        continue;
      }
      any = TRUE;
      eta = real_map_distance(unit_tile(m), city_tile(pcity))
            * SINGLE_MOVE / MAX(unit_type_get(m)->move_rate, 1);
      if (eta <= tune->survival_horizon) {
        return FALSE;           /* One could still come home. */
      }
    }
  }

  return any;
}

/**********************************************************************//**
  Danger a coastal city faces from an invasion the stock assessment
  cannot see: at-war units EMBARKED ON BOATS that could reach the
  city's neighbourhood within about one transport move. assess_danger()
  weighs threats by land movement, so a loaded trireme one strait away
  contributes nothing -- which left deep cities empty in front of boat
  invasions (garden2/isolated mixed games: 2.3x stock's city losses).

  Only cargo counts (a warship shells, it does not take cities), and
  only when it is close enough to land soon: summing every hostile
  unit overseas saturated the danger field and pinned whole
  civilizations at maximum military production (garden2 retest
  2026-07-28: city losses inverted but score 0.861 -> 0.717). Fear
  decays with distance inside the window; both knobs are tuning
  placeholders.
**************************************************************************/
static unsigned int deep_overseas_danger(const struct civ_map *nmap,
                                         struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  struct tile *ptile = city_tile(pcity);
  Continent_id home = tile_continent(ptile);
  double danger = 0.0;

  if (tune->overseas_fear <= 0.0
      || !is_terrain_class_near_tile(nmap, ptile, TC_OCEAN)) {
    return 0;
  }

  players_iterate_alive(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)) {
      continue;
    }
    unit_list_iterate(aplayer->units, aunit) {
      const struct unit *ferry = unit_transport_get(aunit);
      int rating, dist, reach;

      if (ferry == NULL
          || tile_continent(unit_tile(aunit)) == home) {
        /* Not aboard a boat, or already ashore here: assess_danger()
         * covers those. */
        continue;
      }
      rating = adv_unit_att_rating(aunit);
      if (rating <= 0) {
        continue;
      }
      reach = unit_type_get(ferry)->move_rate / SINGLE_MOVE + 1;
      dist = real_map_distance(unit_tile(aunit), ptile);
      if (dist > reach) {
        continue;
      }
      danger += rating * tune->overseas_fear
                / (1.0 + tune->overseas_decay * dist);
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  return (unsigned int) danger;
}

/**********************************************************************//**
  Discount on deep's injected fear terms when the marginal defender
  itself costs upkeep the ruleset charges in gold or food. civ2civ3
  does both (and grants only 1-3 upkeep-free units per city), so every
  requisitioned garrison is a recurring treasury and larder bill;
  classic charges shields and the discount tends to 1. Uses the
  cheapest buildable defending type's government-adjusted upkeep
  (utype_upkeep_cost), so a Democracy fears the doubled bill it
  actually pays.
**************************************************************************/
static double deep_defender_upkeep_discount(const struct civ_map *nmap,
                                            struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  int cheapest = -1;

  if (tune->upkeep_unit_weight <= 0.0) {
    return 1.0;
  }

  unit_type_iterate(ut) {
    int cost;

    if (ut->defense_strength <= 0
        || !can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
      continue;
    }
    cost = utype_upkeep_cost(ut, NULL, pplayer, O_GOLD)
           + utype_upkeep_cost(ut, NULL, pplayer, O_FOOD);
    if (cheapest < 0 || cost < cheapest) {
      cheapest = cost;
    }
  } unit_type_iterate_end;

  if (cheapest <= 0) {
    return 1.0;
  }
  return 1.0 / (1.0 + tune->upkeep_unit_weight * cheapest);
}

/**********************************************************************//**
  Standing fear a coastal city carries while any at-war player OWNS
  transport-capable ships (the "boat shadow"): a sudden trireme gives
  the embarked-cargo term only ~one turn of warning, so exposed
  coasts need a modest garrison and shields BEFORE the boats load --
  and via the CM's danger gate this also tempers grow-hard exactly
  where the garden2b pile regression showed grow-mad shieldless
  coasts dying (ablation 2026-07-30). Zero before any enemy can
  float: fourth application of the no-fear-before-the-threat-class
  law.
**************************************************************************/
static unsigned int deep_boat_shadow_fear(const struct civ_map *nmap,
                                          struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);

  if (tune->boat_shadow_fear <= 0
      || !is_terrain_class_near_tile(nmap, city_tile(pcity), TC_OCEAN)) {
    return 0;
  }

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || !pplayers_at_war(pplayer, aplayer)) {
      continue;
    }
    unit_list_iterate(aplayer->units, aunit) {
      if (unit_type_get(aunit)->transport_capacity > 0
          && utype_move_type(unit_type_get(aunit)) == UMT_SEA) {
        return tune->boat_shadow_fear;
      }
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  return 0;
}

/**********************************************************************//**
  Share of the world's land that nobody has claimed, 0.0 to 1.0, as
  the player can see it.

  deep_map_crowding() and deep_land_pressure() answer from the lobby
  settings and never move for the whole game, so neither can say
  whether there is still anywhere to go. This does: it counts land
  tiles with no owner against all known land. Cached per player per
  turn -- one whole-map pass on a 40x40 is nothing, but it is not
  nothing once per idle settler per turn.

  Honest about vision: a non-omniscient player counts only tiles it
  knows, so unexplored world reads as absent rather than as free. That
  is the conservative direction -- it shrinks the estimate rather than
  inventing room -- and at the difficulties anyone plays the AI is
  omniscient anyway (RC6).
**************************************************************************/
double deep_free_land_share(const struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  bool omniscient = !has_handicap(pplayer, H_MAP);
  int land = 0, free_land = 0;

  if (dp->free_land_turn == game.info.turn) {
    return dp->free_land_permille / 1000.0;
  }

  int own_land = 0;

  whole_map_iterate(&(wld.map), ptile) {
    if (!omniscient && tile_get_known(ptile, pplayer) == TILE_UNKNOWN) {
      continue;
    }
    if (is_ocean_tile(ptile)) {
      continue;
    }
    land++;
    if (tile_owner(ptile) == NULL) {
      free_land++;
    } else if (tile_owner(ptile) == pplayer) {
      own_land++;
    }
  } whole_map_iterate_end;

  dp->free_land_permille = (land > 0 ? 1000 * free_land / land : 0);
  dp->free_land_tiles = free_land;
  dp->own_land_tiles = own_land;
  dp->free_land_turn = game.info.turn;

  return dp->free_land_permille / 1000.0;
}

/**********************************************************************//**
  How many more cities the unclaimed land could still hold, as this
  player packs cities.

  The pool of settlers worth keeping is an inventory of options on
  SITES, so it should be sized by sites remaining, not by cities
  already owned -- a large empire on a full map wants no settlers, and
  a small one on an empty map wants several. Keying off city count got
  that backwards on the roomy cell, where 23 cities drove the target
  down while ~10 settlers were still usefully expanding.

  Density comes from our own empire (own land tiles per own city)
  rather than from citymindist, so it is self-calibrating: it already
  contains whatever spacing the ruleset, the terrain and our own
  objective actually produce. Before we have a city there is nothing to
  calibrate from, so the answer is 0 and the caller falls back to its
  own floor.
**************************************************************************/
double deep_remaining_sites(const struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int cities = city_list_size(pplayer->cities);
  double per_city;

  (void) deep_free_land_share(pplayer);     /* Refreshes the cache. */

  if (cities <= 0 || dp->own_land_tiles <= 0) {
    return 0.0;
  }
  per_city = (double) dp->own_land_tiles / cities;

  return dp->free_land_tiles / MAX(per_city, 1.0);
}

/**********************************************************************//**
  Pre-contact density prior (human refinement 2026-07-30): the lobby
  settings are observable evidence at turn zero. A shared-continent
  startpos (ALL / 2or3 / VARIABLE, including FRACTAL's DEFAULT->ALL
  resolution -- the primary-regime discovery) on thin land means
  early land contact is coming before we can see it. Fear scales
  with crowding, min(1, density_prior_tiles / land-per-player), and
  switches off the moment we meet any non-allied player -- from then
  on the observed density term owns the signal.
**************************************************************************/
double deep_map_crowding(void)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int land_per_player = MAP_INDEX_SIZE * wld.map.server.landpercent
                        / 100 / MAX(player_count(), 1);

  return MIN(1.0, (double) tune->density_prior_tiles
                  / MAX(land_per_player, 1));
}

/**********************************************************************//**
  Land pressure: how scarce settleable land is, on the same evidence as
  deep_map_crowding() and in the same direction, but without its ceiling.

  The crowding prior is a BELIEF -- "early contact is coming" -- so
  clamping it at 1.0 is right: no amount of extra tightness makes a
  certainty more certain. Site scoring asks a different question, "how
  much spacing can we still afford", and that keeps getting worse after
  the belief has saturated. Measured land per player is 80 on
  lategame-80 against a 200-tile reference, so min(1, ref/land) reads
  1.00 there and also reads 1.00 for a map two and a half times roomier;
  the one cell where deep is behind is the one the prior cannot see into.

  ref/(ref + land) is monotone in the same direction, never saturates,
  and stays in (0,1) so weights keep a bounded meaning:

      land/player     80    235    300    400
      min(1,r/land) 1.00   0.85   0.67   0.50
      r/(r+land)    0.71   0.46   0.40   0.33

  Kept separate from deep_map_crowding() rather than replacing it: that
  one has four other consumers whose weights were tuned against its
  shape across two 16-hour campaigns, and redefining it under them would
  silently rescale all of it.
**************************************************************************/
double deep_land_pressure(void)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int land_per_player = MAP_INDEX_SIZE * wld.map.server.landpercent
                        / 100 / MAX(player_count(), 1);

  return (double) tune->land_pressure_tiles
         / (tune->land_pressure_tiles + MAX(land_per_player, 1));
}

/**********************************************************************//**
  Ramp factor for ANTICIPATORY fear (density, prior): scales from 0
  toward 1 over the first density_fear_ramp_turns turns of the game.
  A neighbour's mere existence is not a capability -- deep paid T5
  insurance for a T60 storm in the Europe playtest while classic
  raced naked and armed on contact with actual violence (findings
  2026-07-31, posture timing). Evidence-based terms (menace, boat
  shadow, barbarian onset) are NOT ramped: they already gate on real
  capability signals. Ramp 0 disables (current behaviour).
**************************************************************************/
static double deep_anticipation_ramp(void)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->density_fear_ramp_turns <= 0) {
    return 1.0;
  }
  return MIN(1.0, (double) game.info.turn
                  / tune->density_fear_ramp_turns);
}

static unsigned int deep_density_prior(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  enum map_startpos mode = wld.map.server.startpos;

  if (tune->density_prior_value <= 0) {
    return 0;
  }
  players_iterate_alive(aplayer) {
    if (aplayer != pplayer
        && !pplayers_allied(pplayer, aplayer)
        && player_diplstate_get(pplayer, aplayer)->type
           != DS_NO_CONTACT) {
      return 0;   /* Contact made: observation replaces the prior. */
    }
  } players_iterate_alive_end;

  if (MAPSTARTPOS_DEFAULT == mode) {
    /* Replicate the generator's resolution (mapgen.c). */
    switch (wld.map.server.generator) {
    case MAPGEN_FRACTAL:
      mode = MAPSTARTPOS_ALL;
      break;
    case MAPGEN_ISLAND:
      mode = MAPSTARTPOS_SINGLE;
      break;
    default:
      mode = MAPSTARTPOS_VARIABLE;
      break;
    }
  }
  if (mode == MAPSTARTPOS_SINGLE) {
    return 0;   /* Own continent: land contact is not imminent. */
  }

  return (unsigned int) (tune->density_prior_value
                         * deep_map_crowding()
                         * deep_anticipation_ramp());
}

/**********************************************************************//**
  Chronic border-pressure fear: non-allied cities packed near ours
  are where land wars ignite (cramped is cramped BECAUSE of density),
  so each met non-allied city within density_fear_radius contributes
  density_fear_value / (1 + distance) of standing danger. The land
  sibling of the boat shadow; zero with no neighbours in range, so
  sprawling maps pay nothing.
**************************************************************************/
static unsigned int deep_density_fear(struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  double fear = 0.0;

  if (tune->density_fear_value <= 0) {
    return 0;
  }

  players_iterate_alive(aplayer) {
    int floats = -1;    /* Lazily: does this neighbour own sea lift? */

    if (aplayer == pplayer
        || pplayers_allied(pplayer, aplayer)
        || player_diplstate_get(pplayer, aplayer)->type
           == DS_NO_CONTACT) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      int d = real_map_distance(city_tile(acity), city_tile(pcity));
      double w;

      if (d > tune->density_fear_radius) {
        continue;
      }
      w = tune->density_fear_value / (1.0 + d);
      if (tile_continent(city_tile(acity))
          != tile_continent(city_tile(pcity))) {
        /* Reachability gate (task #18, spec'd 0112a65112 from the
         * Europe playtest: Hannibal hedgehogged against neighbours
         * across a strait none of them could cross). A land army
         * over water is only as frightening as its lift: full
         * weight once the neighbour floats transports, the standing
         * menace_overseas_weight before. */
        if (floats < 0) {
          floats = 0;
          unit_list_iterate(aplayer->units, aunit) {
            const struct unit_type *at = unit_type_get(aunit);

            if (at->transport_capacity > 0
                && utype_move_type(at) == UMT_SEA) {
              floats = 1;
              break;
            }
          } unit_list_iterate_end;
        }
        if (floats == 0) {
          w *= tune->menace_overseas_weight;
        }
      }
      fear += w;
    } city_list_iterate_end;
  } players_iterate_alive_end;

  return (unsigned int) (fear * deep_anticipation_ramp());
}

/**********************************************************************//**
  Flat fear of barbarian uprisings, active from the first turn the
  server can actually spawn them -- the same gate barbarian.c uses --
  and zero before it, so pre-onset production is not taxed. The
  garrison floor insures otherwise-empty cities; this term keeps
  established cities from standing down the moment assessed danger
  returns to zero inside the barbarian era.
**************************************************************************/
static unsigned int deep_barbarian_fear(void)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (BARBS_DISABLED == game.server.barbarianrate
      || game.info.turn < game.server.onsetbarbarian) {
    return 0;
  }
  return tune->barbarian_fear;
}

/**********************************************************************//**
  How militarised the players we have actually met are, relative to
  us, from PUBLIC information only: the units score that the
  historian's periodic "mightiest civilizations" report broadcasts to
  everyone (server/report.c, score.units) -- the travellers' report
  that says keep the gates manned. Ratio of their summed units score
  to ours, through the tunable slope, capped at 1.0. Unmet players
  contribute nothing, so a pre-contact empire keeps no insurance
  garrison at all, and the signal rises BEFORE any war does.

  Each neighbour is weighted by REACHABILITY (the third instance of
  the "no fear before the threat class can exist" law, after the
  barbarian onset gate and the embarked-only overseas term): an army
  that shares a continent with any of our cities counts in full; an
  army wholly overseas counts at menace_overseas_weight, because it
  must first build and sail boats. Both island-native tunes (H, H2)
  independently disabled the ungated floor -- on island maps it was
  pure tax (STATUS 2026-07-30).

  Deliberately count-based and coarse: a chronic background signal,
  not an acute one. Barbarian risk is deliberately not in here;
  deep_barbarian_fear() owns it.
**************************************************************************/
static double deep_garrison_menace(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double theirs = 0.0;
  Continent_id ours[64];
  int n_ours = 0;

  if (tune->garrison_menace_slope <= 0.0) {
    return 1.0;   /* Disabled: unconditional floor. */
  }

  city_list_iterate(pplayer->cities, pcity) {
    Continent_id c = tile_continent(city_tile(pcity));
    int i;

    for (i = 0; i < n_ours; i++) {
      if (ours[i] == c) {
        break;
      }
    }
    if (i == n_ours && n_ours < 64) {
      ours[n_ours++] = c;
    }
  } city_list_iterate_end;

  players_iterate_alive(aplayer) {
    bool reachable = FALSE;

    if (aplayer == pplayer
        || pplayers_allied(pplayer, aplayer)
        || player_diplstate_get(pplayer, aplayer)->type
           == DS_NO_CONTACT) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      Continent_id c = tile_continent(city_tile(acity));

      for (int i = 0; i < n_ours; i++) {
        if (ours[i] == c) {
          reachable = TRUE;
          break;
        }
      }
      if (reachable) {
        break;
      }
    } city_list_iterate_end;
    theirs += aplayer->score.units
              * (reachable ? 1.0 : tune->menace_overseas_weight);
  } players_iterate_alive_end;

  if (theirs <= 0.0) {
    return 0.0;
  }
  return MIN(1.0, tune->garrison_menace_slope * theirs
                  / MAX(pplayer->score.units, 1));
}

/**********************************************************************//**
  Keep a token garrison even where assessed danger is zero. Stock
  cities do this by habit and it silently insures them against every
  threat the assessment cannot see (landings, barbarian spawns); deep
  trusted the assessment literally, left new cities at ndef=0 building
  Settlers, and lost them. The floor makes the insurance explicit and
  tunable instead of removing it, and it is scaled by how militarised
  the neighbours we know about actually are -- zero before contact,
  when the world cannot reach us and every garrison displaces a
  Settler at compounding time.
**************************************************************************/
static void deep_apply_garrison_floor(struct city *pcity,
                                      struct ai_city *city_data,
                                      double upkeep_discount,
                                      double menace)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (city_data->danger == 0
      && unit_list_size(city_tile(pcity)->units) < tune->garrison_floor) {
    city_data->danger
      = (unsigned int) (tune->garrison_floor_danger * upkeep_discount
                        * menace);
  }
}

/**********************************************************************//**
  Sequential-attrition deficit (combat workstream): the linear danger
  sum sits at parity when the garrison is rated equal to the wave, but
  surviving the wave means winning every duel in it, and that
  compounds down (deepodds.c). When the computed survival odds fall
  short of the target, the shortfall funds extra defence -- from
  observed units only; the anticipatory fear terms keep covering what
  cannot be seen. log_it is set by the once-per-turn caller so the
  deep-surv distribution counts each city once.
**************************************************************************/
static unsigned int deep_survival_deficit_fear(const struct civ_map *nmap,
                                               struct city *pcity,
                                               double updisc, bool log_it)
{
  const struct deep_tuning *tune = deep_tuning_get();
  unsigned int fear = 0;
  double surv;
  int wave = 0;

  if (tune->survival_target <= 0.0 || tune->survival_fear_value <= 0.0) {
    return 0;
  }
  surv = deep_city_stack_survival(nmap, pcity, tune->survival_horizon,
                                  &wave);
  if (wave == 0) {
    return 0;
  }
  if (surv < tune->survival_target) {
    double away_mult
      = deep_committed_away(pcity) ? tune->committed_fear_mult : 1.0;

    fear = (unsigned int)
      (tune->survival_fear_value
       * (tune->survival_target - surv) / tune->survival_target
       * updisc * away_mult);
  }
  if (log_it) {
    log_base(deep_metric_log_level(),
             "deep-surv turn=%d player=\"%s\" city=%d p=%.3f wave=%d"
             " fear=%u",
             game.info.turn, player_name(city_owner(pcity)), pcity->id,
             surv, wave, fear);
  }
  return fear;
}

/**********************************************************************//**
  Satisfaction gate (task #19, from the T153 live-game forensics):
  the standing fears keep danger nonzero every turn of a hostile
  game, so the military advisor outbids the granary in every auction
  forever -- 22 deep cities reached T153 with zero growth
  infrastructure while their garrisons were demonstrably adequate.
  Once the garrison is SIZED -- floor met, no urgency or grave
  danger, and survival against the observed wave at target -- the
  injected fears stop bidding (scaled by satisfied_fear_discount,
  default 0: satisfied means quiet, which also lets the danger==0
  rotation and melt hygiene run) and the domestic ladder gets the
  queue. The fears wake the moment the odds slip: the survival check
  sees real approaching threats, including embarked cargo, so
  evidence un-satisfies a city before its fear terms are needed.
**************************************************************************/
static double deep_fear_satisfaction(const struct civ_map *nmap,
                                     struct city *pcity,
                                     const struct ai_city *city_data)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->satisfied_fear_discount >= 1.0) {
    return 1.0;                 /* Gate disabled. */
  }
  if (unit_list_size(city_tile(pcity)->units) < tune->garrison_floor) {
    return 1.0;
  }
  /* Deliberately NOT gated on urgency/grave_danger. Those are
   * presence counts -- any capture-capable enemy within three tiles
   * (one for grave_danger), with no reference to whether the
   * garrison wins -- and they pre-empted the honest test below. In a
   * war they are set on most cities most of the time (Europe T106:
   * 69% of deep's, 86% of stock's), so satisfaction never arrived,
   * every injected fear kept bidding at full strength, and the
   * military choice outbid settlers and growth buildings for the
   * rest of the game. That is the "flips to war mode and never
   * expands again" failure. The survival test below asks the real
   * question over the same horizon those counters scan. */
  if (tune->survival_target > 0.0) {
    int wave = 0;
    double surv = deep_city_stack_survival(nmap, pcity,
                                           tune->survival_horizon,
                                           &wave);

    if (wave > 0 && surv < tune->survival_target) {
      return 1.0;
    }
  }
  return MAX(tune->satisfied_fear_discount, 0.0);
}

/**********************************************************************//**
  Apply the overseas fear, the en-route credit and the garrison floor
  to every city's freshly assessed danger.
**************************************************************************/
static void deep_credit_incoming_defence(struct ai_type *ait,
                                         struct player *pplayer)
{
  double menace = deep_garrison_menace(pplayer);
  /* Gate 1 (W34 A.2). Exactly 1.0 at identity axes, which is what
   * the byte-identity acceptance gate rests on. */
  double fmult = deep_stance_fear_mult(pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    double updisc = deep_defender_upkeep_discount(&(wld.map), pcity);
    double sat = deep_fear_satisfaction(&(wld.map), pcity, city_data);
    unsigned int over = deep_overseas_danger(&(wld.map), pcity);
    unsigned int shadow = deep_boat_shadow_fear(&(wld.map), pcity);
    unsigned int density = deep_density_fear(pcity);
    unsigned int barb = deep_barbarian_fear();
    unsigned int credit;

    if (sat < 1.0) {
      log_base(deep_metric_log_level(),
               "deep-sat turn=%d player=\"%s\" city=%d",
               game.info.turn, player_name(pplayer), pcity->id);
    }
    if (fmult != 1.0 && over + shadow + density + barb > 0) {
      deep_plr_data(pplayer)->stance_fear_gated++;
    }
    city_data->danger += (unsigned int) (over * updisc * sat * fmult);
    city_data->danger += (unsigned int) (shadow * updisc * sat * fmult);
    city_data->danger += (unsigned int) (density * updisc * sat * fmult);
    city_data->danger += (unsigned int) (barb * updisc * sat * fmult);
    /* NOT scaled: the survival deficit prices an OBSERVED wave, and
     * the garrison floor below is the floor gate 1 scales toward. */
    city_data->danger
      += deep_survival_deficit_fear(&(wld.map), pcity, updisc, TRUE);
    credit = deep_incoming_defence_credit(ait, pcity)
             + deep_muster_reserve_credit(ait, pcity);
    city_data->danger -= MIN(city_data->danger, credit);
    deep_apply_garrison_floor(pcity, city_data, updisc, menace);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Melt obsolete surplus garrison (rotation move 3, design in
  findings/civ2civ3-adjustments.md). In a city with zero post-
  adjustment danger and no urgency, holding more own military units
  than the garrison floor, disband the WEAKEST defender -- but only
  when it costs ruleset upkeep and a defender at least
  rotation_power_ratio times stronger is buildable here, so quiet
  turns cannot melt a modern garrison (the obsolescence gate is the
  hysteresis). One action per city per turn; the traditional disband
  recovers shields into production where the ruleset allows (civ2civ3
  does). A Phalanx stack paying gold+food into the Musketeer era was
  the motivating waste.
**************************************************************************/
static void deep_melt_surplus_defenders(struct ai_type *ait,
                                        struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  if (tune->rotation_power_ratio <= 0.0) {
    return;   /* Disabled. */
  }

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct unit *worst = NULL;
    const struct unit_type *wt;
    int worst_def = 0, count = 0, best_buildable = 0;

    if (city_data->urgency > 0 || city_data->danger > 0) {
      continue;
    }
    unit_list_iterate(city_tile(pcity)->units, punit) {
      const struct unit_type *ut = unit_type_get(punit);

      if (unit_owner(punit) != pplayer
          || unit_has_type_flag(punit, UTYF_CIVILIAN)
          || get_transporter_occupancy(punit) > 0
          || ut->defense_strength <= 0
          || ut->attack_strength > ut->defense_strength) {
        /* Attack-heavy units are sortie assets, not garrison fodder:
         * with killstack a garrisoned Cannon erases the besieging
         * stack (human point 2026-07-30). Never melt them here. */
        continue;
      }
      count++;
      if (worst == NULL || ut->defense_strength < worst_def) {
        worst = punit;
        worst_def = ut->defense_strength;
      }
    } unit_list_iterate_end;

    if (worst == NULL || count <= tune->garrison_floor) {
      continue;
    }
    if (VUT_UTYPE == pcity->production.kind) {
      const struct unit_type *pt = pcity->production.value.utype;

      if (pt->defense_strength > 0
          && pt->attack_strength <= pt->defense_strength
          && !utype_has_flag(pt, UTYF_CIVILIAN)) {
        /* NEVER melt a defender to finance another defender (human
         * rule 2026-07-31): the recovered shields flow into current
         * production, and a disband-rebuild cycle burns half the
         * unit's shields for a defense gap -- the seed-205 games
         * melted the same Phalanx into a fresh Phalanx dozens of
         * times. Modernisation is the upgrade-in-place path below;
         * melts may only fund economy, expansion, or offense. */
        continue;
      }
    }
    wt = unit_type_get(worst);
    if (utype_upkeep_cost(wt, NULL, pplayer, O_GOLD)
        + utype_upkeep_cost(wt, NULL, pplayer, O_FOOD)
        + utype_upkeep_cost(wt, NULL, pplayer, O_SHIELD) <= 0) {
      continue;   /* Free to keep; melting gains nothing. */
    }
    if (can_upgrade_unittype(pplayer, wt) == NULL) {
      /* Only units of a PRIOR, OBSOLETED technology may ever be
       * melted (human design intent 2026-07-31): the ruleset's own
       * obsolescence chain is the eligibility test, not any strength
       * comparison. A current-generation defender is never rotation
       * fodder no matter what the ratio arithmetic says. */
      continue;
    }
    unit_type_iterate(ut) {
      if (ut->defense_strength > best_buildable
          && can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
        best_buildable = ut->defense_strength;
      }
    } unit_type_iterate_end;
    {
      /* Recovered shields that would FINISH the current build make a
       * melt more attractive: relax the obsolescence bar (human
       * refinement 2026-07-30). Recover approximated at half the
       * unit's shield cost, the common ruleset rate. */
      double need = tune->rotation_power_ratio;

      if (pcity->shield_stock + unit_build_shield_cost_base(worst) / 2
          >= city_production_build_shield_cost(pcity)) {
        need *= tune->rotation_finish_relax;
      }
      if (best_buildable <= worst_def
          || best_buildable < worst_def * need) {
        /* The strictly-better floor is absolute: no relax or tuned
         * ratio may melt a unit that is not actually obsolete (the
         * seed-205 churn was def=2 best=2 melts sailing under a
         * relaxed bar of 0.76). */
        continue;
      }
    }
    {
      /* Upgrade before disbanding: if the treasury can afford to
       * modernise this unit outright (same reserve-plus-shortfall
       * guard as the spend gate), that beats destroying it. */
      const struct unit_type *up = can_upgrade_unittype(pplayer, wt);

      if (up != NULL) {
        int cost = unit_upgrade_price(pplayer, wt, up);
        int reserve = dai_gold_reserve(pplayer);
        int income = player_get_expected_income(pplayer);
        bool upgraded = FALSE;

        if (income < 0) {
          reserve -= income;
        }
        if (pplayer->economic.gold - cost > reserve) {
          action_id upgrade_actions[MAX_NUM_ACTIONS];
          int i = 0;

          action_array_add_all_by_result(upgrade_actions, &i,
                                         ACTRES_UPGRADE_UNIT);
          action_array_end(upgrade_actions, i);
          action_array_iterate(upgrade_actions, act_id) {
            if (action_ever_possible(act_id)) {
              log_base(deep_metric_log_level(),
                       "deep-rotate turn=%d player=\"%s\" city=%d"
                       " upgrade=\"%s\" to=\"%s\" cost=%d",
                       game.info.turn, player_name(pplayer),
                       pcity->id, unit_rule_name(worst),
                       utype_rule_name(up), cost);
              unit_do_action(pplayer, worst->id, pcity->id, 0, "",
                             act_id);
              upgraded = TRUE;
              break;
            }
          } action_array_iterate_end;
        }
        if (upgraded) {
          continue;   /* Modernised in place; nothing to melt. */
        }
      }
    }
    log_base(deep_metric_log_level(),
             "deep-rotate turn=%d player=\"%s\" city=%d disband=\"%s\""
             " def=%d best=%d count=%d",
             game.info.turn, player_name(pplayer), pcity->id,
             unit_rule_name(worst), worst_def, best_buildable, count);
    unit_do_disband_trad(pplayer, worst, ACT_REQ_PLAYER);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Convoy discipline (naval design in findings/civ2civ3-adjustments.md;
  slices 1+2 shipped together -- a full transport without escort is a
  fatter target than the dribble it replaces).

  HOLD: a sea transport sitting in one of our cities with cargo
  aboard but fewer than convoy_min_load units holds position -- but
  only while at least one own unit within convoy_wait_radius is
  assigned to board it (unit_ai->ferryboat), so the gate cannot
  deadlock: the moment nobody is coming, it sails with what it has.

  ESCORT: any cargo-carrying transport without a bodyguard recruits
  an idle own military sea unit from its tile via the stock bodyguard
  framework, which then moves the pair together.
**************************************************************************/
static void deep_manage_convoys(struct ai_type *ait,
                                struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->convoy_min_load <= 1) {
    return;   /* Disabled: stock single-passenger ferrying. */
  }

  unit_list_iterate(pplayer->units, boat) {
    const struct unit_type *bt = unit_type_get(boat);
    struct city *pcity;
    int cargo, want;

    if (bt->transport_capacity <= 0
        || utype_move_type(bt) != UMT_SEA) {
      continue;
    }
    pcity = tile_city(unit_tile(boat));
    if (pcity == NULL || city_owner(pcity) != pplayer) {
      continue;
    }
    cargo = get_transporter_occupancy(boat);
    if (cargo <= 0) {
      continue;
    }

    if (aiguard_guard_of(ait, boat) == NULL) {
      unit_list_iterate(unit_tile(boat)->units, w) {
        const struct unit_type *wt = unit_type_get(w);
        struct unit_ai *wd = def_ai_unit_data(w, ait);

        if (unit_owner(w) == pplayer && w != boat
            && utype_move_type(wt) == UMT_SEA
            && wt->transport_capacity == 0
            && wt->attack_strength > 0
            && wd->task == AIUNIT_NONE && wd->charge <= 0) {
          aiguard_assign_guard_unit(ait, boat, w);
          log_base(deep_metric_log_level(),
                   "deep-convoy turn=%d player=\"%s\" escort=\"%s\""
                   " boat=%d cargo=%d",
                   game.info.turn, player_name(pplayer),
                   unit_rule_name(w), boat->id, cargo);
          break;
        }
      } unit_list_iterate_end;
    }

    want = MIN(get_transporter_capacity(boat), tune->convoy_min_load);
    if (cargo >= want) {
      /* Loaded to the bar: sail. The massed-landing decision this
       * branch once tried to make (naval slice 3's port scan) never
       * fired -- stock ferries board and sail within one manage
       * pass, so a loaded convoy resting in port does not exist at
       * phase time. The invade gate lives in the operation layer's
       * crossing shadow now (deepops.c), at the seam where a
       * landing is actually decided. */
      continue;
    }
    {
      bool incoming = FALSE;

      unit_list_iterate(pplayer->units, p) {
        if (def_ai_unit_data(p, ait)->ferryboat == boat->id
            && !same_pos(unit_tile(p), unit_tile(boat))
            && real_map_distance(unit_tile(p), unit_tile(boat))
               <= tune->convoy_wait_radius) {
          incoming = TRUE;
          break;
        }
      } unit_list_iterate_end;
      if (incoming) {
        deep_hold_unit(ait, boat, "convoy");
        log_base(deep_metric_log_level(),
                 "deep-convoy turn=%d player=\"%s\" hold boat=%d"
                 " cargo=%d want=%d",
                 game.info.turn, player_name(pplayer), boat->id,
                 cargo, want);
      }
    }
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Kill sortie (combat E3, human design 2026-07-31): a garrison blade
  steps out to finish a weak or weakened besieger, even though the
  city spends the night one defender short. Stock defenders rampage
  only above BODYGUARD_RAMPAGE_THRESHOLD * 5 in kill_desire currency
  -- too timid, and the wrong arithmetic. Deep's gate is two
  probabilities: the best adjacent duel must clear sortie_win_floor,
  and the city without this unit must still clear survival_target
  against the observed wave (an attacker erased today is subtracted
  from every future wave). Tired blades stay home -- a sortie at
  scaled-down power is the exact mistake the tired-attack pass
  exists to stop. One sortie per city per turn.
**************************************************************************/
static void deep_kill_sorties(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  if (tune->sortie_win_floor <= 0.0) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    struct unit *blade = NULL;
    const struct unit *victim = NULL;
    double best = 0.0;

    unit_list_iterate(city_tile(pcity)->units, u) {
      if (unit_owner(u) != pplayer
          || unit_has_type_flag(u, UTYF_CIVILIAN)
          || u->hp <= 0
          || u->moves_left < SINGLE_MOVE
          || def_ai_unit_data(u, ait)->done
          || adv_unit_att_rating(u) <= 0) {
        continue;
      }
      adjc_iterate(nmap, city_tile(pcity), atile) {
        struct unit *pdef = get_defender(nmap, u, atile, NULL);
        double chance;

        if (pdef == NULL
            || !pplayers_at_war(pplayer, unit_owner(pdef))
            || (!is_barbarian(unit_owner(pdef))
                && player_diplstate_get(pplayer, unit_owner(pdef))->type
                   == DS_NO_CONTACT)) {
          /* Unmet neighbours are nominally at war (initial
           * diplstate); picking that fight is diplomacy's call, not
           * the sortie pass's. */
          continue;
        }
        chance = unit_win_chance(nmap, u, pdef, NULL);
        if (chance > best) {
          best = chance;
          blade = u;
          victim = pdef;
        }
      } adjc_iterate_end;
    } unit_list_iterate_end;

    if (blade == NULL || best < tune->sortie_win_floor) {
      continue;
    }
    if (deep_city_stack_survival_without(nmap, pcity,
                                         tune->survival_horizon,
                                         blade, NULL)
        < tune->survival_target) {
      continue;
    }
    log_base(deep_metric_log_level(),
             "deep-sortie turn=%d player=\"%s\" city=%d kill=\"%s\""
             " victim=\"%s\" p=%.3f",
             game.info.turn, player_name(pplayer), pcity->id,
             unit_rule_name(blade), unit_rule_name(victim), best);
    /* Rampage executes the attack(s) with all the action legality
     * handling; the pair of gates above is what stock never had. */
    (void) dai_military_rampage(blade, RAMPAGE_ANYTHING,
                                RAMPAGE_FREE_CITY_OR_BETTER);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Cover sortie (combat E3, human design 2026-07-31): a threatened
  working civilian whose bodyguard request nobody has answered can
  borrow a blade from a nearby city's garrison -- the escort arrives
  from two tiles away this turn instead of from a build queue in
  five. The lender must still clear survival_target without the
  blade, the weakest sparable defender goes (its absence hurts the
  stack least), and a city lends at most one per turn so the joint
  check stays honest. The guard framework walks the pair from here.
**************************************************************************/
static void deep_cover_sorties(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  if (tune->worker_guard_radius <= 0 || tune->survival_target <= 0.0) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    struct unit *charge = NULL, *lend = NULL;
    int bestd = tune->worker_guard_radius + 1;
    int lend_rating = 0;

    unit_list_iterate(pplayer->units, w) {
      int d;

      if (!unit_has_type_flag(w, UTYF_CIVILIAN)
          || unit_transported(w)
          || tile_city(unit_tile(w)) != NULL
          || !aiguard_wanted(ait, w)
          || tile_continent(unit_tile(w))
             != tile_continent(city_tile(pcity))) {
        continue;
      }
      d = real_map_distance(city_tile(pcity), unit_tile(w));
      if (d < bestd) {
        bestd = d;
        charge = w;
      }
    } unit_list_iterate_end;
    if (charge == NULL) {
      continue;
    }

    unit_list_iterate(city_tile(pcity)->units, g) {
      int rating = adv_unit_def_rating_basic(g);

      if (unit_owner(g) != pplayer
          || unit_has_type_flag(g, UTYF_CIVILIAN)
          || g->hp <= 0
          || rating <= 0
          || def_ai_unit_data(g, ait)->done
          || aiguard_has_charge(ait, g)) {
        continue;
      }
      if (lend != NULL && rating >= lend_rating) {
        continue;   /* Lend the weakest sparable blade. */
      }
      if (deep_city_stack_survival_without(nmap, pcity,
                                           tune->survival_horizon,
                                           g, NULL)
          < tune->survival_target) {
        continue;
      }
      lend = g;
      lend_rating = rating;
    } unit_list_iterate_end;

    if (lend != NULL) {
      log_base(deep_metric_log_level(),
               "deep-sortie turn=%d player=\"%s\" city=%d cover=\"%s\""
               " charge=\"%s\" dist=%d",
               game.info.turn, player_name(pplayer), pcity->id,
               unit_rule_name(lend), unit_rule_name(charge), bestd);
      aiguard_assign_guard_unit(ait, charge, lend);
    }
  } city_list_iterate_end;
}

/**********************************************************************//**
  Tired-attack discipline (combat workstream): under
  game.info.tired_attack, striking with moves_left < SINGLE_MOVE
  scales attack power by moves_left/SINGLE_MOVE (combat.c) -- an
  attack on one-third moves is an attack at one-third strength, a
  trade the stock AI never prices. A unit on an attack mission,
  standing adjacent to its target with only a tired strike left,
  holds this turn and strikes fresh next -- unless the tired odds
  already clear tired_attack_floor (a wounded straggler does not earn
  a night's rest). The hold is one turn of initiative given up for
  roughly SINGLE_MOVE/moves_left more strength; the knob referees.
**************************************************************************/
static void deep_hold_tired_attackers(struct ai_type *ait,
                                      struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  if (!game.info.tired_attack || tune->tired_attack_floor <= 0.0) {
    return;
  }

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *unit_data = def_ai_unit_data(punit, ait);
    struct unit *pdefender;
    double chance;

    if (unit_data->task != AIUNIT_ATTACK
        || punit->goto_tile == NULL
        || punit->moves_left <= 0
        || punit->moves_left >= SINGLE_MOVE
        || real_map_distance(unit_tile(punit), punit->goto_tile) != 1) {
      continue;
    }
    pdefender = get_defender(nmap, punit, punit->goto_tile, NULL);
    if (pdefender == NULL
        || !pplayers_at_war(pplayer, unit_owner(pdefender))) {
      /* Nothing (visible) to duel: a walk-in capture needs no rest,
       * and a stale mission is the manager's to re-decide. */
      continue;
    }
    chance = unit_win_chance(nmap, punit, pdefender, NULL);
    if (chance >= tune->tired_attack_floor) {
      continue;
    }
    log_base(deep_metric_log_level(),
             "deep-tired turn=%d player=\"%s\" unit=\"%s\" moves=%d"
             " p=%.3f at=(%d,%d)",
             game.info.turn, player_name(pplayer),
             unit_rule_name(punit), punit->moves_left, chance,
             TILE_XY(unit_tile(punit)));
    deep_hold_unit(ait, punit, "tired");
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Interrupt working civilians in danger and run them to the nearest
  own city (human request 2026-07-30: a mid-transform Engineer keeps
  digging as the Knights ride up -- stock auto-workers fear PATHS but
  never abandon an activity). A civilian outside a city with an
  at-war military unit within worker_flee_radius drops its activity,
  legs it to the closest own city on its continent, and is marked
  done for the turn so nothing re-tasks it into the field.
**************************************************************************/
static void deep_flee_workers(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->worker_flee_radius <= 0) {
    return;   /* Disabled. */
  }

  unit_list_iterate_safe(pplayer->units, w) {
    bool danger = FALSE;
    struct city *refuge = NULL;
    int best = FC_INFINITY;

    if (!unit_has_type_flag(w, UTYF_CIVILIAN)
        || unit_transported(w)
        || tile_city(unit_tile(w)) != NULL) {
      continue;
    }
    {
      /* A bodyguard standing with us is the answer to danger, not a
       * reason to abandon work: guarded civilians hold their ground
       * (the flee would waste the escort the border-guard system
       * just delivered). */
      struct unit *g = aiguard_guard_of(ait, w);

      if (g != NULL
          && real_map_distance(unit_tile(g), unit_tile(w)) <= 1) {
        continue;
      }
    }
    players_iterate_alive(aplayer) {
      if (!pplayers_at_war(pplayer, aplayer)) {
        continue;
      }
      unit_list_iterate(aplayer->units, e) {
        if (adv_unit_att_rating(e) > 0
            && real_map_distance(unit_tile(e), unit_tile(w))
               <= tune->worker_flee_radius) {
          danger = TRUE;
          break;
        }
      } unit_list_iterate_end;
      if (danger) {
        break;
      }
    } players_iterate_alive_end;
    if (!danger) {
      /* No immediate threat: but a civilian WORKING an exposed tile
       * (unowned ground, or an at-war unit within the wider guard
       * radius) requests a bodyguard through the stock framework, so
       * escorts arrive BEFORE the flee radius is breached (human
       * request 2026-07-30: prioritise protection at the border). */
      if (tune->worker_guard_radius > 0
          && !aiguard_has_guard(ait, w)
          && w->activity != ACTIVITY_IDLE) {
        bool exposed = tile_owner(unit_tile(w)) != pplayer;

        if (!exposed) {
          players_iterate_alive(aplayer) {
            if (!pplayers_at_war(pplayer, aplayer)) {
              continue;
            }
            unit_list_iterate(aplayer->units, e) {
              if (adv_unit_att_rating(e) > 0
                  && real_map_distance(unit_tile(e), unit_tile(w))
                     <= tune->worker_guard_radius) {
                exposed = TRUE;
                break;
              }
            } unit_list_iterate_end;
            if (exposed) {
              break;
            }
          } players_iterate_alive_end;
        }
        if (exposed) {
          aiguard_request_guard(ait, w);
          log_base(deep_metric_log_level(),
                   "deep-guard turn=%d player=\"%s\" unit=\"%s\"",
                   game.info.turn, player_name(pplayer),
                   unit_rule_name(w));
        }
      }
      continue;
    }
    city_list_iterate(pplayer->cities, pcity) {
      int d = real_map_distance(city_tile(pcity), unit_tile(w));

      if (d < best
          && tile_continent(city_tile(pcity))
             == tile_continent(unit_tile(w))) {
        best = d;
        refuge = pcity;
      }
    } city_list_iterate_end;
    if (refuge == NULL) {
      continue;
    }
    if (w->goto_tile == city_tile(refuge)) {
      /* Already running there: keep running, no re-path spam. */
      deep_hold_unit(ait, w, "flee");
      continue;
    }
    log_base(deep_metric_log_level(),
             "deep-flee turn=%d player=\"%s\" unit=\"%s\" to=%d dist=%d",
             game.info.turn, player_name(pplayer), unit_rule_name(w),
             refuge->id, best);
    unit_activity_handling(w, ACTIVITY_IDLE, ACTION_NONE);
    if (dai_unit_goto(ait, w, city_tile(refuge))) {
      deep_hold_unit(ait, w, "flee");
    }
  } unit_list_iterate_safe_end;
}

/**********************************************************************//**
  Defence counter-claim (findings/defence-counter-claim.md): a city
  below the survival bar under an observed wave claims ONE defender
  per turn -- the same one-per-turn discipline as the sorties, so
  the joint checks stay honest and the credit machinery (W11) can
  absorb each arrival before the next is summoned. Recruits, nearest
  first: free land defenders arriving within the survival horizon,
  then raiders (released from the hunt -- the sit-tight idlers are
  exactly who this wants), then a mustering op's blade when the
  city's expected loss outbids the op's expected take. The recruit
  travels on the stock bodyguard framework (ESCORT + city charge),
  which self-releases when the danger passes.
**************************************************************************/
/* Relay search bound: cities examined in one brigade. A bookkeeping
 * cap, not a tunable -- the BFS takes the shortest chain and stops
 * at the first genuinely sparable garrison. */
#define DEEP_RELAY_MAX 64
/* Cap on one city's relief force in a turn (bookkeeping). */
#define DEEP_RELIEF_MAX 12

/**********************************************************************//**
  The weakest blade `lender` can send: own military, not bodyguarding
  or op-claimed. With `must_stay_safe`, the lender must still clear
  survival_target without it (the terminal link of a brigade, which
  gets no backfill); intermediate links skip that check because the
  same pass refills them from behind.
**************************************************************************/
static struct unit *deep_city_sparable_blade(struct ai_type *ait,
                                             struct city *lender,
                                             bool must_stay_safe)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct player *pplayer = city_owner(lender);
  struct unit *spare = NULL;

  unit_list_iterate(city_tile(lender)->units, g) {
    if (unit_owner(g) != pplayer
        || unit_has_type_flag(g, UTYF_CIVILIAN)
        || g->hp <= 0
        || adv_unit_def_rating_basic(g) <= 0
        || utype_move_type(unit_type_get(g)) != UMT_LAND
        || def_ai_unit_data(g, ait)->charge > 0
        || deep_op_claiming(pplayer, g->id) >= 0) {
      continue;
    }
    if (spare != NULL
        && adv_unit_def_rating_basic(g)
           >= adv_unit_def_rating_basic(spare)) {
      continue;                 /* Send the weakest blade. */
    }
    if (must_stay_safe
        && deep_city_stack_survival_without(nmap, lender,
                                            tune->survival_horizon,
                                            g, NULL)
           < tune->survival_target) {
      continue;                 /* The lender needs it tonight. */
    }
    spare = g;
  } unit_list_iterate_end;

  return spare;
}

/**********************************************************************//**
  Find a BRIGADE that could reinforce `pcity` (human design
  2026-08-01). Marching one blade five turns from the nearest safe
  city is strictly worse than five blades each stepping one city
  along a connected chain: the front is reinforced now, every city
  in the middle ends net even (sends one forward, receives one from
  behind), and the hole migrates to the far, safe end.

  BFS from pcity over own cities on its continent, hops of at most
  survival_horizon tiles, THROUGH cities holding any movable blade,
  stopping at the first that can spare one and stay above the bar.
  Fills `path` back-to-front (path[0] = pcity) and returns its
  length, or 0 when no chain terminates in a genuinely sparable
  garrison -- a brigade that would only shuffle the hole between two
  unsafe cities is never started.
**************************************************************************/
static int deep_relay_chain(struct ai_type *ait, struct player *pplayer,
                            struct city *pcity, struct city **path)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct city *q[DEEP_RELAY_MAX];
  int parent[DEEP_RELAY_MAX];
  int nq = 0, head = 0, found = -1, idx, i, len = 0;

  q[nq] = pcity;
  parent[nq] = -1;
  nq++;

  while (head < nq && found < 0) {
    struct city *cur = q[head];

    city_list_iterate(pplayer->cities, other) {
      bool seen = FALSE;
      int d;

      if (nq >= DEEP_RELAY_MAX) {
        break;
      }
      for (i = 0; i < nq; i++) {
        if (q[i] == other) {
          seen = TRUE;
          break;
        }
      }
      if (seen
          || tile_continent(city_tile(other))
             != tile_continent(city_tile(cur))) {
        continue;
      }
      d = real_map_distance(city_tile(cur), city_tile(other));
      if (d > tune->survival_horizon) {
        continue;               /* Not one hop for this brigade. */
      }
      if (deep_city_sparable_blade(ait, other, TRUE) != NULL) {
        q[nq] = other;
        parent[nq] = head;
        found = nq++;
        break;                  /* Shortest chain wins. */
      }
      if (deep_city_sparable_blade(ait, other, FALSE) != NULL) {
        q[nq] = other;
        parent[nq] = head;
        nq++;                   /* Can relay, cannot terminate. */
      }
    } city_list_iterate_end;
    head++;
  }

  if (found < 0) {
    return 0;
  }
  for (idx = found; idx >= 0; idx = parent[idx]) {
    len++;
  }
  i = len;
  for (idx = found; idx >= 0; idx = parent[idx]) {
    path[--i] = q[idx];
  }

  return len;
}

/**********************************************************************//**
  Walk a brigade: one blade moves per edge, back to front. The
  terminal garrison sends a blade it can genuinely spare; every
  intermediate sends any movable blade because the same pass refills
  it from behind.
**************************************************************************/
static void deep_relay_execute(struct ai_type *ait, struct player *pplayer,
                               struct city **path, int len)
{
  int i;

  for (i = len - 1; i > 0; i--) {
    struct city *from = path[i], *to = path[i - 1];
    struct unit *blade = deep_city_sparable_blade(ait, from,
                                                  i == len - 1);

    if (blade == NULL) {
      continue;                 /* Raced away; the rest still moves. */
    }
    log_base(deep_metric_log_level(),
             "deep-defend turn=%d player=\"%s\" city=%d claim=\"%s\""
             " src=relay hop=%d from=%d",
             game.info.turn, player_name(pplayer), to->id,
             unit_rule_name(blade), i, from->id);
    aiguard_assign_guard_city(ait, to, blade);
    dai_unit_new_task(ait, blade, AIUNIT_ESCORT, city_tile(to));
  }
}

static void deep_defend_counter_claims(struct ai_type *ait,
                                       struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  if (tune->survival_target <= 0.0) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    struct unit *cand[DEEP_RELIEF_MAX], *force[DEEP_RELIEF_MAX];
    struct city *backfill[DEEP_RELIEF_MAX];
    int n_cand = 0, n_force = 0, i, j;
    int wave = 0;
    double p, relieved;

    p = deep_city_stack_survival(nmap, pcity, tune->survival_horizon,
                                 &wave);
    if (wave == 0 || p >= tune->survival_target) {
      continue;
    }

    /* Every blade that could stand here in time, from any pool the
     * defence may draw on. */
    unit_list_iterate(pplayer->units, u) {
      struct unit_ai *ud = def_ai_unit_data(u, ait);
      struct city *needs_backfill = NULL;
      int oi, rate, eta;

      if (n_cand >= DEEP_RELIEF_MAX) {
        break;
      }
      if (u->hp <= 0
          || unit_has_type_flag(u, UTYF_CIVILIAN)
          || adv_unit_def_rating_basic(u) <= 0
          || utype_move_type(unit_type_get(u)) != UMT_LAND
          || unit_transported(u)
          || tile_continent(unit_tile(u))
             != tile_continent(city_tile(pcity))
          || same_pos(unit_tile(u), city_tile(pcity))
          || ud->charge > 0) {
        continue;
      }
      oi = deep_op_claiming(pplayer, u->id);
      if (oi >= 0
          && deep_plr_data(pplayer)->ops[oi].kind != DEEP_OPK_RAID) {
        continue;   /* Assault claims answer through the poach bid. */
      }
      rate = MAX(unit_type_get(u)->move_rate, 1);
      eta = (real_map_distance(unit_tile(u), city_tile(pcity))
             * SINGLE_MOVE + rate - 1) / rate;
      if (eta > tune->survival_horizon) {
        continue;   /* Would arrive after the wave. */
      }
      if (ud->task == AIUNIT_DEFEND_HOME) {
        /* Another city's garrison. It goes if that city can spare it
         * -- or if a brigade can refill that city behind it, which
         * is how a blade five cities away arrives tonight instead of
         * in five turns. */
        struct city *home = tile_city(unit_tile(u));
        struct city *chain[DEEP_RELAY_MAX];

        if (home == NULL || city_owner(home) != pplayer) {
          continue;
        }
        if (deep_city_stack_survival_without(nmap, home,
                                             tune->survival_horizon,
                                             u, NULL)
            < tune->survival_target) {
          if (deep_relay_chain(ait, pplayer, home, chain) == 0) {
            continue;   /* Its own night needs it and nobody backs it. */
          }
          needs_backfill = home;
        }
      }
      cand[n_cand] = u;
      backfill[n_cand] = needs_backfill;
      n_cand++;
    } unit_list_iterate_end;

    /* Strongest first: a relief force is sized, not scraped. */
    for (i = 1; i < n_cand; i++) {
      struct unit *cu = cand[i];
      struct city *cb = backfill[i];

      for (j = i; j > 0
           && adv_unit_def_rating_basic(cand[j - 1])
              < adv_unit_def_rating_basic(cu); j--) {
        cand[j] = cand[j - 1];
        backfill[j] = backfill[j - 1];
      }
      cand[j] = cu;
      backfill[j] = cb;
    }

    /* Take blades until the city would actually hold. Sending fewer
     * is the trickle the operations layer refuses on the attacking
     * side: units fed in one at a time die one at a time. If the
     * whole reachable pool cannot clear the bar, send NOBODY -- the
     * triage falls out of the arithmetic, not a new knob. What to do
     * with a city written off this way (delay, or evacuate) is
     * W14's, and a human call: findings/defence-counter-claim.md. */
    relieved = p;
    for (i = 0; i < n_cand && relieved < tune->survival_target; i++) {
      force[n_force++] = cand[i];
      relieved = deep_city_stack_survival_with(nmap, pcity,
                                               tune->survival_horizon,
                                               force, n_force, NULL);
    }
    if (relieved < tune->survival_target && n_force < DEEP_RELIEF_MAX) {
      struct unit *poach = deep_op_poach_defender(ait, pplayer, pcity, p);

      if (poach != NULL) {
        backfill[n_force] = NULL;
        force[n_force++] = poach;
        relieved = deep_city_stack_survival_with(nmap, pcity,
                                                 tune->survival_horizon,
                                                 force, n_force, NULL);
      }
    }
    if (relieved < tune->survival_target) {
      log_base(deep_metric_log_level(),
               "deep-defend turn=%d player=\"%s\" city=%d writeoff"
               " p=%.3f wave=%d pool=%d best=%.3f",
               game.info.turn, player_name(pplayer), pcity->id, p, wave,
               n_cand, relieved);
      continue;   /* Cannot be held tonight: do not feed it blades. */
    }

    for (i = 0; i < n_force; i++) {
      struct unit *u = force[i];

      if (deep_op_claiming(pplayer, u->id) >= 0) {
        deep_op_release_member(pplayer, u->id);
      }
      log_base(deep_metric_log_level(),
               "deep-defend turn=%d player=\"%s\" city=%d claim=\"%s\""
               " src=%s p=%.3f wave=%d force=%d relieved=%.3f",
               game.info.turn, player_name(pplayer), pcity->id,
               unit_rule_name(u),
               i < n_cand && backfill[i] != NULL ? "brigade"
                 : (def_ai_unit_data(u, ait)->task == AIUNIT_DEFEND_HOME
                    ? "lend" : "free"),
               p, wave, n_force, relieved);
      aiguard_assign_guard_city(ait, pcity, u);
      dai_unit_new_task(ait, u, AIUNIT_ESCORT, city_tile(pcity));
      if (i < n_cand && backfill[i] != NULL) {
        struct city *chain[DEEP_RELAY_MAX];
        int len = deep_relay_chain(ait, pplayer, backfill[i], chain);

        if (len > 0) {
          deep_relay_execute(ait, pplayer, chain, len);
        }
      }
    }
  } city_list_iterate_end;
}

/**********************************************************************//**
  Activities before unit movement; derived from
  dai_do_first_activities() with defence bookkeeping added between the
  danger assessment and the unit management that consumes it.
**************************************************************************/
void deep_do_first_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  dai_assess_danger_player(ait, &(wld.map), pplayer);

  deep_log_defence(ait, pplayer);
  deep_credit_incoming_defence(ait, pplayer);
  deep_melt_surplus_defenders(ait, pplayer);
  deep_manage_convoys(ait, pplayer);
  deep_hold_tired_attackers(ait, pplayer);
  deep_flee_workers(ait, pplayer);
  deep_kill_sorties(ait, pplayer);
  deep_cover_sorties(ait, pplayer);
  deep_defend_counter_claims(ait, pplayer);
  deep_run_operations(ait, pplayer);
  deep_set_city_manager_params(ait, pplayer);

  TIMING_LOG(AIT_UNITS, TIMER_START);
  /* The stock loop with op holds honoured -- dai_manage_units()
   * would clear the done flags the operations just set. */
  deep_manage_units(ait, pplayer);
  TIMING_LOG(AIT_UNITS, TIMER_STOP);
  /* STOP.  Everything else is at end of turn. */

  TIMING_LOG(AIT_ALL, TIMER_STOP);

  flush_packets(); /* AIs can be such spammers... */
}

/**********************************************************************//**
  Should this city stop thinking about settlers and workers entirely?

  Stock asks dai_on_war_footing(), which is TRUE if ANY player carries a
  war countdown >= 0 -- a presence flag on a counter stock sets and does
  not reliably clear. When it sticks, every city with any military want
  has founder_want AND worker_want forced to zero and skips the
  contemplation step outright, so the player never builds another
  settler or worker for the rest of the game.

  Measured in the live Europe game, T125-T126 (findings/europe-T80.md):
  16 of Deep_Carthago's 18 cities never reached contemplation at all,
  founder_want pinned at 0 alongside worker_want at 0 -- the pair is the
  gate's signature -- while the one city that escaped it wanted a
  settler at 514. Zeroing every city's shield_stock and replaying the
  turn changed nothing, which rules out the production change penalty
  and leaves this gate as the cause. Carthage sat on a 624-tile island,
  alone, with 180 workable unclaimed tiles.

  It is also the FIFTH instance of this project's recurring defect:
  a presence count used where a probability is available (the four
  fixed on 2026-08-01 are in STATUS.md). Deep owns a survival model, so
  ask the question the gate is really for -- is THIS city in danger --
  rather than "does a stale counter exist anywhere in the world".

  expansion_war_survival <= 0 restores stock's behaviour exactly.
**************************************************************************/
/**********************************************************************//**
  Hulls that are genuinely SPARE, for damping the colony boat want.

  Deliberately not `stats.available_boats`, which counts every ferry
  whose `unit_data->passenger` is FERRY_AVAILABLE (`daiferry.c:113`) --
  that is "nobody is assigned to me", not "I have nothing to do". A
  boat out exploring the coastline is unassigned and busy, and damping
  shipbuilding because the fleet is away working is precisely backwards
  (human, 2026-08-03).

  Excluded here: anything carrying cargo, anything an operation has
  claimed (the lift drives its boat directly and never sets
  `passenger`, so its hull reads as available all through a muster),
  and anything exploring under either the AI task or the server-side
  agent.

  Only the SIGN of `passenger` is read, the same convention
  deep_lift_wants_boat() uses for `ferryboat`: FERRY_AVAILABLE is -1
  and private to daiferry.c, and an assigned passenger is a real unit
  id, so `>= 0` means taken.
**************************************************************************/

/**********************************************************************//**
  Units standing on land touching this water that want a crossing: the
  DEMAND one body of water has to serve.

  Pairing this against the idle hulls on the same water is what makes
  "how many boats do we need" answerable at all. A ratio damping cannot
  target a number -- it only ever says "fewer than now" -- and cutting
  the fleet globally measured as fewer deliveries with the idle
  fraction pinned at 75% either way.
**************************************************************************/
int deep_ferry_demand_on(struct ai_type *ait, struct player *pplayer,
                         Continent_id ocean)
{
  return deep_ferry_demand_near(ait, pplayer, ocean, NULL, NULL);
}

/**********************************************************************//**
  The same demand, with its DISTANCE reported.

  A count alone says "somebody wants to cross this water", and on an
  island map one body of water can be the whole world sea -- so an
  idle hull "with demand on its water" may be half a map from the unit
  waiting. Where `from` is given, `*nearest` receives the map distance
  to the closest waiting unit (unchanged when there is none), which is
  what separates a hull that ignored a passenger at the next quay from
  one that was simply somewhere else.
**************************************************************************/
int deep_ferry_demand_near(struct ai_type *ait, struct player *pplayer,
                           Continent_id ocean, const struct tile *from,
                           int *nearest)
{
  const struct civ_map *nmap = &(wld.map);
  int want = 0;

  unit_list_iterate(pplayer->units, punit) {
    bool touches = FALSE;

    if (unit_transported(punit)
        || def_ai_unit_data(punit, ait)->ferryboat >= 0
        || punit->goto_tile == NULL
        || tile_continent(unit_tile(punit))
           == tile_continent(punit->goto_tile)) {
      continue;                 /* Not waiting for a crossing. */
    }
    adjc_iterate(nmap, unit_tile(punit), atile) {
      if (tile_continent(atile) == ocean) {
        touches = TRUE;
        break;
      }
    } adjc_iterate_end;
    if (touches) {
      want++;
      if (from != NULL && nearest != NULL) {
        int d = real_map_distance(from, unit_tile(punit));

        if (*nearest < 0 || d < *nearest) {
          *nearest = d;
        }
      }
    }
  } unit_list_iterate_end;

  return want;
}

int deep_idle_ferries_on(struct ai_type *ait, struct player *pplayer,
                         Continent_id ocean);
int deep_ferry_demand_on(struct ai_type *ait, struct player *pplayer,
                         Continent_id ocean);

/**********************************************************************//**
  Does this body of water touch more than one land continent -- i.e.
  can a hull launched into it ever put a passenger ashore anywhere
  other than where it embarked?

  This is the question the boat want has never asked. It is gated on
  `is_terrain_class_near_tile(TC_OCEAN)`, "am I coastal", and a lake or
  a sea that closes on one island is indistinguishable there from the
  world ocean. Per the hull census, one hull in five never once saw a
  waiting passenger on its water in its whole life.

  Deliberately GEOMETRY and not demand: 84% of idle hull-turns have no
  waiting passenger on that water because demand is bursty, so a
  "nobody wants it now" test would suppress the hulls that serve the
  next burst -- the same mistake five count-based dampings made. This
  question does not move with demand at all.

  No `home` argument is needed, and that is not an omission: if a water
  touches exactly one continent then every city adjacent to it stands
  on that same continent, so a single-surrounder water is useless to
  every city that could build into it.

  The map already keeps the answer. `assign_continent_numbers()` calls
  `recalculate_surrounders()` (mapgen_utils.c:237), which is re-run on
  terrain change, and `get_lake_surrounder()` reports the single
  continent around a water, or a negative value when there are several.
  00-context section 3 lists this among the infrastructure built and
  never read.
**************************************************************************/
bool deep_water_bridges(Continent_id ocean)
{
  if (ocean >= 0 || -ocean > wld.map.num_oceans) {
    return FALSE;                     /* Not a body of water at all. */
  }

  /* < 0: several distinct continents on this water. 0: no land known
   * to touch it. > 0: exactly one, so both ends of any crossing are
   * the same landmass. */
  return get_lake_surrounder(ocean) < 0;
}

/**********************************************************************//**
  Does any water this tile touches lead to another continent?
**************************************************************************/
bool deep_city_water_bridges(const struct tile *ptile)
{
  adjc_iterate(&(wld.map), ptile, atile) {
    if (deep_water_bridges(tile_continent(atile))) {
      return TRUE;
    }
  } adjc_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  Spare hulls on the waters this tile touches: idle beyond the units
  waiting to cross them, summed over each distinct body of water.
**************************************************************************/
int deep_water_surplus(struct ai_type *ait, struct player *pplayer,
                       const struct tile *ptile)
{
  Continent_id seen[8];
  int nseen = 0, surplus = 0;

  adjc_iterate(&(wld.map), ptile, atile) {
    Continent_id oc = tile_continent(atile);
    int si;

    if (oc >= 0) {
      continue;
    }
    for (si = 0; si < nseen; si++) {
      if (seen[si] == oc) {
        break;
      }
    }
    if (si < nseen || nseen >= (int) ARRAY_SIZE(seen)) {
      continue;
    }
    seen[nseen++] = oc;
    surplus += MAX(0, deep_idle_ferries_on(ait, pplayer, oc)
                      - deep_ferry_demand_on(ait, pplayer, oc));
  } adjc_iterate_end;

  return surplus;
}

static int deep_idle_ferries(struct ai_type *ait, struct player *pplayer)
{
  return deep_idle_ferries_on(ait, pplayer, 0);
}

/**********************************************************************//**
  Idle hulls ON ONE BODY OF WATER (a negative continent id), or on all
  of them when `ocean` is 0.

  Counting the whole fleet was the wrong denominator, and the measured
  reason is that the idle FRACTION does not move with fleet size:
  quartering the boat want took the fleet 5.32 -> 4.57 hulls per
  player-turn and left idleness at exactly 75% both times, while
  deliveries fell. A fleet that is 75% idle at every size is not
  overbuilt, it is badly DISTRIBUTED -- you need nine hulls to have one
  where the demand is, because a boat on the wrong sea cannot serve any
  of it.

  So the damping asks the question a shipyard can actually act on: how
  many idle hulls are already on MY water. A city on a crowded sea
  stops building; a city on an empty one keeps going. `adv->oceans[]`
  has carried this decomposition every phase since forever with only
  `.threat` ever read (00-context section 3).
**************************************************************************/
int deep_idle_ferries_on(struct ai_type *ait, struct player *pplayer,
                         Continent_id ocean)
{
  int idle = 0;

  unit_list_iterate(pplayer->units, punit) {
    const struct unit_ai *ud = def_ai_unit_data(punit, ait);

    if (!dai_is_ferry(punit, ait)
        || get_transporter_occupancy(punit) > 0
        || ud->passenger >= 0
        || ud->task == AIUNIT_EXPLORE
        || punit->ssa_controller == SSA_AUTOEXPLORE
        || deep_op_claiming(pplayer, punit->id) >= 0) {
      continue;
    }
    if (ocean != 0) {
      /* A hull IN PORT sits on the city's LAND tile, so matching the
       * unit's own continent against an ocean id excluded exactly the
       * boats that are idle -- the count came back ~0 and the damping
       * never fired at all (the logged want was identical with it on
       * and off). A boat belongs to the water it touches. */
      bool touches = (tile_continent(unit_tile(punit)) == ocean);

      if (!touches) {
        adjc_iterate(&(wld.map), unit_tile(punit), atile) {
          if (tile_continent(atile) == ocean) {
            touches = TRUE;
            break;
          }
        } adjc_iterate_end;
      }
      if (!touches) {
        continue;
      }
    }
    idle++;
  } unit_list_iterate_end;

  return idle;
}

static bool deep_expansion_blocked(struct ai_type *ait,
                                   struct player *pplayer,
                                   struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int wave = 0;
  double surv;

  if (tune->expansion_war_survival <= 0.0) {
    return dai_on_war_footing(ait, pplayer);
  }
  if (!dai_on_war_footing(ait, pplayer)) {
    return FALSE;
  }
  /* On war footing, but only cities that would actually FALL stop
   * planning for the future. A safe city in a warring empire still
   * settles and still improves its land. */
  surv = deep_city_stack_survival(&(wld.map), pcity,
                                  tune->survival_horizon, &wave);
  return wave > 0 && surv < tune->expansion_war_survival;
}

/**********************************************************************//**
  Military build choice for one city. Derived from
  military_advisor_choose_build() in daimilitary.c (every helper is
  called, not copied); the difference is that the en-route defender
  credit is applied between the danger assessment and the decisions
  that consume it -- the stock function re-runs assess_danger()
  internally, which is why adjusting danger at phase start cannot
  affect production.
**************************************************************************/
static struct adv_choice *deep_military_choose_build(struct ai_type *ait,
                                                     const struct civ_map *nmap,
                                                     struct player *pplayer,
                                                     struct city *pcity,
                                                     player_unit_list_getter ul_cb)
{
  struct adv_data *ai = adv_data_get(pplayer, NULL);
  struct unit_type *punittype;
  unsigned int our_def, urgency;
  struct tile *ptile = pcity->tile;
  struct unit *virtualunit;
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  adv_want martial_value = 0;
  bool martial_need = FALSE;
  struct adv_choice *choice = adv_new_choice();
  bool allow_gold_upkeep;

  urgency = assess_danger(ait, nmap, pcity, ul_cb);
  /* Changing to quadratic to stop AI from building piles
   * of small units -- Syela */
  /* It has to be AFTER assess_danger() thanks to wallvalue. */
  our_def = assess_defense_quadratic(ait, pcity);

  {
    /* Fear invasions the assessment cannot see, then count the help
     * that is already on its way, so the city does not requisition
     * replacements for it (W11), and never fall below the token
     * garrison. All injected fear is discounted by what the ruleset
     * charges to keep the marginal defender. */
    double updisc = deep_defender_upkeep_discount(nmap, pcity);
    double sat = deep_fear_satisfaction(nmap, pcity, city_data);
    /* Gate 1, the same multiplier the once-per-turn bookkeeping
     * applies: the danger the build choice reads must not disagree
     * with the danger the ledger holds. */
    double fmult = deep_stance_fear_mult(pplayer);
    unsigned int over = deep_overseas_danger(nmap, pcity);
    unsigned int shadow = deep_boat_shadow_fear(nmap, pcity);
    unsigned int density = deep_density_fear(pcity);
    unsigned int prior = deep_density_prior(pplayer);
    unsigned int barb = deep_barbarian_fear();
    unsigned int credit;

    if (fmult != 1.0 && over + shadow + density + prior + barb > 0) {
      deep_plr_data(pplayer)->stance_fear_gated++;
    }
    city_data->danger += (unsigned int) (over * updisc * sat * fmult);
    city_data->danger += (unsigned int) (shadow * updisc * sat * fmult);
    city_data->danger += (unsigned int) (density * updisc * sat * fmult);
    city_data->danger += (unsigned int) (prior * updisc * sat * fmult);
    city_data->danger += (unsigned int) (barb * updisc * sat * fmult);
    /* NOT scaled: see the once-per-turn site. */
    city_data->danger
      += deep_survival_deficit_fear(nmap, pcity, updisc, FALSE);
    credit = deep_incoming_defence_credit(ait, pcity)
             + deep_muster_reserve_credit(ait, pcity);
    city_data->danger -= MIN(city_data->danger, credit);
    deep_apply_garrison_floor(pcity, city_data, updisc,
                              deep_garrison_menace(pplayer));

    if (city_data->danger == 0 && deep_tuning_get()->rotation_want > 0
        && deep_tuning_get()->rotation_power_ratio > 0.0) {
      /* Rotation move 2 (replace-then-disband): if the BEST defender
       * present is obsolete by the rotation ratio, inject a small
       * want so the city queues a modern one; the melt (move 3)
       * removes the old unit only once the new one raises the count
       * above the floor -- new arrives before old leaves, zero
       * garrison gap. */
      int best_present = 0, best_buildable = 0;

      unit_list_iterate(ptile->units, punit) {
        const struct unit_type *put = unit_type_get(punit);

        if (unit_owner(punit) == pplayer
            && !unit_has_type_flag(punit, UTYF_CIVILIAN)
            && put->defense_strength > best_present) {
          best_present = put->defense_strength;
        }
      } unit_list_iterate_end;
      unit_type_iterate(ut) {
        if (ut->defense_strength > best_buildable
            && can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
          best_buildable = ut->defense_strength;
        }
      } unit_type_iterate_end;
      if (best_present > 0
          && best_buildable
             >= best_present * deep_tuning_get()->rotation_power_ratio) {
        city_data->danger = (unsigned int)
          (deep_tuning_get()->rotation_want * updisc);
      }
    }
  }

  {
    /* Defensive diplomat (W01): stock writes a self-described
     * 'totally arbitrary' 16000 into the choice, outranking even
     * military emergencies by magnitude. Evaluate it on a fresh
     * choice and merge it back as an emergency-tier want with a
     * calibrated value instead. */
    struct adv_choice *dip = adv_new_choice();

    dai_choose_diplomat_defensive(ait, pplayer, pcity, dip, our_def);
    if (dip->want > 0) {
      deep_want_apply(dip, (struct deep_want) {
        .tier = DWT_EMERGENCY,
        .value = deep_tuning_get()->diplomat_defence_value });
      if (deep_want_cmp(deep_want_from_choice(dip),
                        deep_want_from_choice(choice)) > 0) {
        adv_choice_copy(choice, dip);
      }
    }
    adv_free_choice(dip);
  }

  if (pcity->feel[CITIZEN_UNHAPPY][FEELING_NATIONALITY]
      + pcity->feel[CITIZEN_ANGRY][FEELING_NATIONALITY] > 0) {
    martial_need = TRUE;
  }

  if (!martial_need) {
    normal_specialist_type_iterate(sp) {
      if (pcity->specialists[sp] > 0
          && get_specialist_output(pcity, sp, O_LUXURY) > 0) {
        martial_need = TRUE;
        break;
      }
    } normal_specialist_type_iterate_end;
  }

  if (martial_need
      && unit_list_size(pcity->tile->units)
         < get_city_bonus(pcity, EFT_MARTIAL_LAW_MAX)) {
    martial_value
      = dai_content_effect_value(pplayer, pcity,
                                 get_city_bonus(pcity,
                                                EFT_MARTIAL_LAW_BY_UNIT),
                                 1, FEELING_FINAL);
  }

  /* Otherwise no need to defend yet */
  if (city_data->danger != 0 || martial_value > 0) {
    struct impr_type *pimprove;
    int num_defenders = unit_list_size(ptile->units);
    int wall_id, danger;
    bool build_walls = TRUE;
    bool def_unit_selected = FALSE;
    int qdanger = city_data->danger * city_data->danger;

    if (qdanger <= 0) {
      /* We only need these defenders because of Martial Law value */
      danger = 0;
      build_walls = FALSE; /* Walls don't provide Martial Law */
    } else {
      /* First determine the danger. It is measured in percents of our
       * defensive strength, capped at 200 + urgency */
      if (qdanger >= our_def) {
        if (urgency == 0) {
          /* Don't waste money */
          danger = 100;
        } else if (our_def == 0) {
          danger = 200 + urgency;
        } else {
          danger = MIN(200, 100 * qdanger / our_def) + urgency;
        }
      } else {
        danger = 100 * qdanger / our_def;
      }

      if (pcity->surplus[O_SHIELD] <= 0 && our_def != 0) {
        /* Won't be able to support anything */
        danger = 0;
      }
    }

    CITY_LOG(LOG_DEBUG, pcity, "d_m_c_b urgency=%d danger=%d num_def=%d "
             "our_def=%d", urgency, danger, num_defenders, our_def);

    if (our_def == 0 && danger > 0) {
      /* Build defensive unit first! Walls will help nothing if there's
       * nobody behind them. */
      if (dai_process_defender_want(ait, nmap, pplayer, pcity, danger,
                                    choice, martial_value)) {
        deep_want_apply(choice, (struct deep_want) {
          .tier = DWT_EMERGENCY, .value = danger });
        adv_choice_set_use(choice, "first defender");
        build_walls = FALSE;
        def_unit_selected = TRUE;

        CITY_LOG(LOG_DEBUG, pcity,
                 "d_m_c_b wants first defender with " ADV_WANT_PRINTF,
                 choice->want);
      }
    }
    if (build_walls) {
      /* We will build walls if we can and want and (have "enough"
       * defenders or can just buy the walls straight away) */

      /* HACK: This needs changing if multiple improvements provide
       * this effect. */
      wall_id = dai_find_source_building(pcity, EFT_DEFEND_BONUS, NULL);
      pimprove = improvement_by_number(wall_id);

      if (wall_id != B_LAST && deep_tuning_get()->wall_fear_weight > 0.0
          && city_data->danger > 0) {
        /* Deep's injected fears (boat shadow, density, menace) fund
         * WALLS as well as bodies: walls are the upkeep-free defence,
         * the capital-for-labour answer in gold-upkeep rulesets
         * (human idea 2026-07-30). */
        pcity->server.adv->building_want[wall_id]
          += deep_tuning_get()->wall_fear_weight * city_data->danger;
      }

      if (wall_id != B_LAST
          && pcity->server.adv->building_want[wall_id] != 0 && our_def != 0
          && can_city_build_improvement_now(pcity, pimprove, RPT_CERTAIN)
          && (danger < 101 || num_defenders > 1
              || (city_data->grave_danger == 0
                  && pplayer->economic.gold
                     > impr_buy_gold_cost(pcity, pimprove,
                                          pcity->shield_stock)))
          && ai_fuzzy(pplayer, TRUE)) {
        if (pcity->server.adv->building_want[wall_id] > 0) {
          /* NB: great wall is under domestic */
          choice->value.building = pimprove;
          /* building_want is hacked by assess_danger() */
          choice->want = pcity->server.adv->building_want[wall_id];
          choice->want = choice->want
            * (0.5 + (ai_trait_get_value(TRAIT_BUILDER, pplayer)
                      / TRAIT_DEFAULT_VALUE / 2));
          if (urgency == 0
              && deep_want_from_choice(choice).tier == DWT_EMERGENCY) {
            /* No urgency: cap the wall want just below the tier
             * boundary (stock capped at exactly the boundary value;
             * the from/apply round trip preserves that number). */
            deep_want_apply(choice, (struct deep_want) {
              .tier = DWT_EMERGENCY, .value = 0 });
          }
          choice->type = CT_BUILDING;
          adv_choice_set_use(choice, "defense building");
          CITY_LOG(LOG_DEBUG, pcity,
                   "d_m_c_b wants defense building with " ADV_WANT_PRINTF,
                   choice->want);
        } else {
          build_walls = FALSE;
        }
      } else {
        build_walls = FALSE;
      }
    }

    /* If our choice isn't defender unit already, consider one */
    if (!def_unit_selected) {
      if ((danger > 0 && num_defenders <= urgency) || martial_value > 0) {
        struct adv_choice uchoice;

        adv_init_choice(&uchoice);

        /* Consider building defensive units */
        if (dai_process_defender_want(ait, nmap, pplayer, pcity, danger,
                                      &uchoice, martial_value)) {
          /* Potential defender found */
          if (urgency == 0
              && uchoice.value.utype->defense_strength == 1) {
            /* FIXME: check other reqs (unit class?) */
            if (get_city_bonus(pcity, EFT_HP_REGEN) > 0) {
              /* Unlikely */
              uchoice.want = MIN(49, danger);
            } else {
              uchoice.want = MIN(25, danger);
            }
          } else {
            uchoice.want = danger;
          }

          uchoice.want += martial_value;
          if (danger > 0) {
            adv_choice_set_use(&uchoice, "defender");
          } else {
            adv_choice_set_use(&uchoice, "police");
          }

          if (!build_walls || uchoice.want > choice->want) {
            adv_choice_copy(choice, &uchoice);
          }
          adv_deinit_choice(&uchoice);

          CITY_LOG(LOG_DEBUG, pcity,
                   "d_m_c_b wants %s with desire " ADV_WANT_PRINTF,
                   utype_rule_name(choice->value.utype),
                   choice->want);
        } else {
          CITY_LOG(LOG_DEBUG, pcity, "d_m_c_b cannot select defender");
        }
      } else {
        CITY_LOG(LOG_DEBUG, pcity, "d_m_c_b does not want defenders");
      }
    }
  } /* Ok, don't need to defend */

  if (pcity->surplus[O_SHIELD] <= 0
      || pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL]
         > pcity->feel[CITIZEN_UNHAPPY][FEELING_EFFECT]
      || pcity->id == ai->wonder_city) {
    /* Things we consider below are not life-saving so we don't want to
     * build them if our populace doesn't feel like it */
    return choice;
  }

  if (pplayer->economic.tax <= 50
      || city_total_unit_gold_upkeep(pcity) <= 0) {
    /* Always allow one unit with real gold upkeep (after
     * EFT_UNIT_UPKEEP_FREE_PER_CITY). Allow more if economics is so
     * strong that we have not increased taxes. */
    allow_gold_upkeep = TRUE;
  } else {
    allow_gold_upkeep = FALSE;
  }

  /* Consider making a land bodyguard */
  punittype = dai_choose_bodyguard(ait, nmap, pcity, TC_LAND,
                                   L_DEFEND_GOOD, allow_gold_upkeep);
  if (punittype) {
    dai_unit_consider_bodyguard(ait, nmap, pcity, punittype, choice);
  }

  /* If we are in severe danger, don't consider attackers. This is
     probably too general. In many cases we will want to buy attackers
     to counterattack. -- Per */
  {
    /* Severe danger: an emergency-tier want that stays emergency even
     * after discounting the martial-law share (stock compared
     * want - martial >= tier base + 0.1). */
    struct deep_want w = deep_want_from_choice(choice);

    if (w.tier == DWT_EMERGENCY && w.value - martial_value >= 0.1
        && city_data->grave_danger > 0) {
    CITY_LOG(LOGLEVEL_BUILD, pcity,
             "severe danger (want " ADV_WANT_PRINTF "), force defender",
             choice->want);
      return choice;
    }
  }

  /* Consider making an offensive diplomat. Under Embassies tech
   * leakage (civ2civ3), every missing embassy is forgone research
   * discount, so the diplomat's want gains embassy_science_value per
   * met non-allied player we lack an embassy with -- but only when
   * the offensive evaluation actually picked a diplomat-class unit
   * (W06-adjacent; stock wants embassy diplomats already, this
   * prices the leakage the stock want has never heard of). */
  {
    struct adv_choice *dip = adv_new_choice();

    dai_choose_diplomat_offensive(ait, nmap, pplayer, pcity, dip);
    if (dip->want > 0
        && game.info.tech_leakage == TECH_LEAKAGE_EMBASSIES
        && deep_tuning_get()->embassy_science_value > 0.0) {
      int missing = 0;

      players_iterate_alive(aplayer) {
        bool reachable = FALSE;

        if (aplayer == pplayer
            || pplayers_allied(pplayer, aplayer)
            || player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT
            || player_has_embassy(pplayer, aplayer)) {
          continue;
        }
        /* Only count embassies a diplomat could actually be
         * DELIVERED to: a shared continent, or any sea transport to
         * carry it. An unreachable embassy is an unsatisfiable want,
         * and unsatisfiable wants pump units forever (seed 205 built
         * 11 diplomats on an island with no boats; same disease as
         * the explorer pump, same cure). */
        if (def_ai_player_data(pplayer, ait)->stats.boats > 0) {
          reachable = TRUE;
        } else {
          city_list_iterate(aplayer->cities, tcity) {
            city_list_iterate(pplayer->cities, ocity) {
              if (tile_continent(city_tile(tcity))
                  == tile_continent(city_tile(ocity))) {
                reachable = TRUE;
                break;
              }
            } city_list_iterate_end;
            if (reachable) {
              break;
            }
          } city_list_iterate_end;
        }
        if (reachable) {
          missing++;
        }
      } players_iterate_alive_end;
      dip->want += missing * deep_tuning_get()->embassy_science_value;
    }
    if (dip->want > choice->want) {
      adv_choice_copy(choice, dip);
    }
    adv_free_choice(dip);
  }

  /* Consider making a sea bodyguard */
  punittype = dai_choose_bodyguard(ait, nmap, pcity, TC_OCEAN,
                                   L_DEFEND_GOOD, allow_gold_upkeep);
  if (punittype) {
    dai_unit_consider_bodyguard(ait, nmap, pcity, punittype, choice);
  }

  /* Consider making an airplane */
  (void) dai_choose_attacker_air(ait, nmap, pplayer, pcity, choice,
                                 allow_gold_upkeep);

  /* Consider making a paratrooper */
  dai_choose_paratrooper(ait, nmap, pplayer, pcity, choice,
                         allow_gold_upkeep);

  /* Check if we want a sailing attacker. Have to put sailing first
     before we mung the seamap */
  punittype = dai_choose_attacker(ait, nmap, pcity, TC_OCEAN,
                                  allow_gold_upkeep);
  if (punittype) {
    virtualunit = unit_virtual_create(
      pplayer, pcity, punittype,
      city_production_unit_veteran_level(pcity, punittype));
    choice = kill_something_with(ait, nmap, pplayer, pcity, virtualunit,
                                 choice);
    unit_virtual_destroy(virtualunit);
  }

  /* Consider a land attacker or a ferried land attacker
   * (in which case, we might want a ferry before an attacker)
   */
  punittype = dai_choose_attacker(ait, nmap, pcity, TC_LAND,
                                  allow_gold_upkeep);
  if (punittype) {
    /* The third producer of ferry bids, and the one the water gate's
     * backstop caught at 1.4 per game: when the victim is across
     * water, kill_something_with() substitutes a TRANSPORT for the
     * attacker it wanted (daimilitary.c:1687). There is no scalar to
     * zero the way founder_want withdraws the domestic bid -- the
     * decision is taken inside a frozen function on a ferry_map it
     * builds itself -- so the withdrawal happens at that producer's
     * boundary instead: keep the choice as it stood going in, and put
     * it back if what comes out is a hull for water that reaches no
     * other continent. That is not reconstructing a discarded
     * runner-up; it is an alternative we are still holding.
     *
     * Scoped to the LAND call deliberately. The sea-attacker call
     * above can also return a ferry-typed hull, but as a WARSHIP, and
     * the gate's claim says nothing about fighting on a closed sea --
     * only about carrying a passenger off this continent. */
    struct adv_choice pre_attacker;

    adv_init_choice(&pre_attacker);
    adv_choice_copy(&pre_attacker, choice);
    virtualunit = unit_virtual_create(pplayer, pcity, punittype, 1);
    choice = kill_something_with(ait, nmap, pplayer, pcity, virtualunit,
                                 choice);
    unit_virtual_destroy(virtualunit);
    if (deep_tuning_get()->boat_gate_at_bid > 0
        && deep_boat_choice_refused(ait, choice, pcity)) {
      log_base(deep_metric_log_level(),
               "deep-watergate-attack turn=%d player=\"%s\" city=%d"
               " type=\"%s\" want=" ADV_WANT_PRINTF,
               game.info.turn, player_name(pplayer), pcity->id,
               utype_rule_name(choice->value.utype), choice->want);
      adv_choice_copy(choice, &pre_attacker);
    }
    adv_deinit_choice(&pre_attacker);
    /* Deep's own pricing runs beside the stock evaluation and the
     * larger want wins; distributions of the two are compared via
     * the deep-kill/deep-want tripwires before stock is cut out. */
    deep_assault_want(ait, nmap, pplayer, pcity, punittype, choice);
  }

  /* Colony lift (task #20): a lift-limited settler evaluation left a
   * fresh overseas-demand score (deepsettler.c); a coastal city
   * answers it by building the boat. This is the same
   * lift-limited-means-build-the-boat rule the invasion side uses,
   * pointed at settlement -- it breaks the 0-boats/0-demand deadlock
   * that left 71% of the Europe map unowned. */
  {
    struct deep_plr *dp = deep_plr_data(pplayer);
    int lift_demand = 0;

    if (dp->colony_want > 0 && dp->colony_turn >= game.info.turn - 1) {
      /* Damped by hulls ALREADY IDLE (human, 2026-08-03). colony_want
       * is a pure site-quality number -- the best overseas result a
       * settler can see -- and nothing between it and the shipyard
       * asked whether the fleet could already serve it. Measured over
       * 20 games on lategame-80: 1.63 boats per player-turn of which
       * 1.19 sit idle (73%), only 18% carrying anything, against 0.53
       * waiting passengers -- 3.1 hulls each, and the carrying share
       * FALLING from 23.9% to 13.0% as the fleet grew. That is
       * overbuilding, and it is invisible to a want that cannot see
       * its own fleet.
       *
       * A good site does not become a worse site because a boat is
       * idle -- but the marginal HULL is worth less, which is what
       * this want actually buys. boat_idle_damping = 0 restores the
       * undamped signal exactly and is the A/B switch. */
      /* Idle hulls on THIS city's water, not in the whole empire: a
       * boat sitting on another sea cannot serve the demand this city
       * would be building for. Measured, the empire-wide idle
       * fraction is 75% at every fleet size, so the empire-wide count
       * damps a shipyard for hulls it can never use. */
      double idle = 0;
      Continent_id seen[8];
      int nseen = 0;

      adjc_iterate(nmap, ptile, atile) {
        Continent_id oc = tile_continent(atile);
        int si;

        if (oc >= 0) {
          continue;             /* Land. */
        }
        for (si = 0; si < nseen; si++) {
          if (seen[si] == oc) {
            break;
          }
        }
        if (si < nseen || nseen >= (int) ARRAY_SIZE(seen)) {
          continue;
        }
        seen[nseen++] = oc;
        /* SURPLUS on this water, not raw idleness: hulls already idle
         * beyond the passengers waiting to use them. Damping on idle
         * alone suppressed building while demand went unserved, which
         * is how a 75%-idle fleet still lost deliveries when it was
         * cut. One spare hull per waiting passenger is the target this
         * expresses; anything past that is what gets damped. */
        idle += MAX(0, deep_idle_ferries_on(ait, pplayer, oc)
                       - deep_ferry_demand_on(ait, pplayer, oc));
      } adjc_iterate_end;

      lift_demand
        = (int) (dp->colony_want
                 / (1.0 + deep_tuning_get()->boat_idle_damping * idle));
      {
        /* Gate 4's other half: a settler that cannot cross is not an
         * expansion, so the hull the colony needs is priced by the
         * same appetite as the settler itself. Applied to the COLONY
         * demand only -- the amphibious muster's berth shortfall
         * below is military lift and belongs to no economic gate. */
        double emult = deep_stance_expand_mult(pplayer);

        if (emult != 1.0 && lift_demand > 0) {
          lift_demand = (int) (lift_demand * emult);
          deep_plr_data(pplayer)->stance_expand_gated++;
        }
      }
    }
    {
      /* An amphibious muster short on hulls is lift demand too
       * (naval slice 3): count the fleet's berths against the
       * combined required force of every overseas operation. */
      int oi, needed = 0, worth = 0;

      for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
        const struct deep_operation *dop = &dp->ops[oi];
        struct city *ot;

        if (dop->state != DEEP_OP_MUSTER) {
          continue;
        }
        ot = game_city_by_number(dop->target_city);
        if (ot != NULL
            && tile_continent(city_tile(ot)) != tile_continent(ptile)) {
          needed += dop->required;
          worth = MAX(worth, (int) def_ai_city_data(ot, ait)->worth);
        }
      }
      if (needed > 0) {
        int berths = 0;

        unit_list_iterate(pplayer->units, b) {
          const struct unit_type *btt = unit_type_get(b);

          if (btt->transport_capacity > 0
              && utype_move_type(btt) == UMT_SEA) {
            berths += btt->transport_capacity;
          }
        } unit_list_iterate_end;
        if (berths < needed) {
          lift_demand = MAX(lift_demand, worth);
        }
      }
    }

    /* "Am I coastal" (is_terrain_class_near_tile) is the wrong
     * question and always was: a hull is useful only if its water
     * touches the demand as well as this city. Asking WHICH water
     * here withdraws the bid at formation rather than vetoing it
     * after it has won a build slot. */
    if (deep_tuning_get()->colony_lift_weight > 0.0
        && lift_demand > 0
        && is_terrain_class_near_tile(nmap, ptile, TC_OCEAN)
        && (deep_tuning_get()->boat_gate_at_bid <= 0
            || deep_tuning_get()->boat_water_gate <= 0
            || deep_city_water_bridges(ptile))) {
      struct unit_type *boattype
        = best_role_unit_for_player(pplayer, L_FERRYBOAT);

      if (boattype != NULL
          && can_city_build_unit_now(nmap, pcity, boattype,
                                     RPT_CERTAIN)) {
        int bt = utype_build_shield_cost(pcity, NULL, boattype)
                 / MAX(pcity->surplus[O_SHIELD], 1);
        adv_want w
          = amortize(deep_tuning_get()->colony_lift_weight
                     * lift_demand, MAX(1, bt));

        log_base(deep_metric_log_level(),
                 "deep-colony turn=%d player=\"%s\" city=%d lift=\"%s\""
                 " want=" ADV_WANT_PRINTF,
                 game.info.turn, player_name(pplayer), pcity->id,
                 utype_rule_name(boattype), w);
        if (w > choice->want) {
          choice->value.utype = boattype;
          choice->want = w;
          choice->type = CT_ATTACKER;
          adv_choice_set_use(choice, "colony lift");
        }
      }
    }
  }

  /* Consider a hunter */
  dai_hunter_choice(ait, pplayer, pcity, choice, allow_gold_upkeep);

  /* Consider veteran level enhancing buildings before non-urgent
   * units -- but never before BODIES. Stock swaps the unit choice
   * for a Barracks with no garrison check at all, even in an empty
   * city (its own walls logic guards against exactly this; the
   * veterancy path never got the memo), which filled early queues
   * with academies for armies that did not exist (human observation,
   * Europe T153). The floor and grave-danger gates are the deep
   * rule: a sized garrison may invest in veterancy, a bare one
   * builds the soldier. */
  {
    int floor_def = 0;

    unit_list_iterate(ptile->units, fu) {
      if (unit_owner(fu) == pplayer
          && !unit_has_type_flag(fu, UTYF_CIVILIAN)) {
        floor_def++;
      }
    } unit_list_iterate_end;
    if (floor_def >= MAX(deep_tuning_get()->garrison_floor, 1)
        && city_data->grave_danger == 0) {
      adjust_ai_unit_choice(pcity, choice);
    }
  }

  if (choice->want <= 0) {
    CITY_LOG(LOGLEVEL_BUILD, pcity, "deep military advisor has no advice");
  } else {
    CITY_LOG(LOGLEVEL_BUILD, pcity,
             "deep military advisor choice: %s (want " ADV_WANT_PRINTF ")",
             adv_choice_rule_name(choice),
             choice->want);
  }

  return choice;
}

/**********************************************************************//**
  Marginal-take-odds attacker want (combat E2). Stock kill_desire
  arithmetic saturates linearly where sequential combat compounds --
  the horde-of-weak-units bug (readme-audit 2.5). Deep prices the
  build directly: pick the best-value at-war city on this continent,
  and value one more attacker of this type by how much it moves
  P(our committed pool takes that garrison), times the target's
  worth, amortized over the march. The marginal form is self-limiting
  everywhere the linear form pumps: hopeless targets and complete
  forces both price the next unit near zero, and a nearly-sufficient
  force surges up the S-curve to completion. The pool is priced as
  arriving together (deep_take_odds); making that true is the
  operations layer's job (E6), so this is the assembled ceiling.
**************************************************************************/
static void deep_assault_want(struct ai_type *ait,
                              const struct civ_map *nmap,
                              struct player *pplayer, struct city *pcity,
                              struct unit_type *punittype,
                              struct adv_choice *choice)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct city *target = NULL;
  double best_score = 0.0, p0, p1, marginal;
  Continent_id home = tile_continent(city_tile(pcity));
  struct unit *pool[32];      /* deep_take_odds caps the stream anyway */
  int n_pool = 0, eta, rate, csize;
  struct unit *vunit;
  adv_want tworth, want;

  if (tune->attack_value_weight <= 0.0 || punittype == NULL) {
    return;
  }

  {
    /* Overseas targets are eligible once we float (or could float)
     * lift; the convoy gates own the launch decision (naval slice
     * 3). Landlocked targets across water stay off the menu. */
    bool can_sail
      = def_ai_player_data(pplayer, ait)->stats.boats > 0
        || best_role_unit_for_player(pplayer, L_FERRYBOAT) != NULL;

    players_iterate_alive(aplayer) {
      /* The initial diplstate between UNMET players is war, so the
       * at-war test alone passes on turn one -- and unmet,
       * ungarrisoned cities then price P(take)=1 and outbid every
       * settler on the queue (contested run 2026-07-31, median
       * 0.758: worth*1.0 wants of ~190 from turn 1). No contact, no
       * target; unknown tile, no target. The offensive mirror of the
       * no-fear-before-the-threat law. */
      if (!pplayers_at_war(pplayer, aplayer)
          || player_diplstate_get(pplayer, aplayer)->type
             == DS_NO_CONTACT) {
        continue;
      }
      city_list_iterate(aplayer->cities, acity) {
        double score;
        bool overseas = tile_continent(city_tile(acity)) != home;

        if (!map_is_known(city_tile(acity), pplayer)
            || (overseas
                && (!can_sail
                    || !is_terrain_class_near_tile(nmap,
                                                   city_tile(acity),
                                                   TC_OCEAN)))) {
          continue;
        }
        score = def_ai_city_data(acity, ait)->worth
                / (1.0 + real_map_distance(city_tile(pcity),
                                           city_tile(acity)));
        if (score > best_score) {
          best_score = score;
          target = acity;
        }
      } city_list_iterate_end;
    } players_iterate_alive_end;
  }
  if (target == NULL) {
    return;
  }
  tworth = def_ai_city_data(target, ait)->worth;

  /* The committed pool: own attackers on this continent not tied to
   * home defence. Counting them all as one assault is exactly the
   * simultaneity assumption documented above. */
  unit_list_iterate(pplayer->units, punit) {
    if (n_pool >= (int) ARRAY_SIZE(pool) - 1) {
      break;
    }
    if (punit->hp <= 0
        || unit_has_type_flag(punit, UTYF_CIVILIAN)
        || adv_unit_att_rating(punit) <= 0
        || tile_continent(unit_tile(punit)) != home
        || def_ai_unit_data(punit, ait)->task == AIUNIT_DEFEND_HOME) {
      continue;
    }
    pool[n_pool++] = punit;
  } unit_list_iterate_end;

  {
    /* March or sail time is the defender's budget on BOTH terrains
     * (one law): the marginal odds are priced against the garrison
     * as it will be at arrival. */
    bool overseas
      = tile_continent(city_tile(target)) != home;

    if (overseas) {
      const struct unit_type *ferrytype
        = best_role_unit_for_player(pplayer, L_FERRYBOAT);

      rate = MAX(ferrytype != NULL ? ferrytype->move_rate : SINGLE_MOVE,
                 1);
    } else {
      rate = MAX(punittype->move_rate, 1);
    }
    eta = (real_map_distance(city_tile(pcity), city_tile(target))
           * SINGLE_MOVE + rate - 1) / rate + (overseas ? 2 : 0);
  }

  vunit = unit_virtual_create(pplayer, pcity, punittype, 1);
  p0 = n_pool > 0
       ? deep_take_odds_at_arrival(nmap, target, pool, n_pool, eta)
       : 0.0;
  pool[n_pool] = vunit;
  p1 = deep_take_odds_at_arrival(nmap, target, pool, n_pool + 1, eta);
  unit_virtual_destroy(vunit);
  marginal = p1 - p0;

  csize = 0;
  if (tune->op_take_target > 0.0 && p1 < tune->op_take_target) {
    /* Campaign pricing (T160 forensics): the S-curve bottom is
     * self-strangling from a standing start -- against any real
     * garrison, attacker #1 moves P(take) by ~0, so a nation with
     * no offensive arm never grows one (at T160 every army in
     * Europe was defensive infantry while 47 empty barbarian cities
     * sat unclaimed). Price the campaign instead: the smallest
     * force of this type that clears op_take_target values each
     * member at the average rate -- the muster shortfall becomes
     * build demand, the land analogue of lift-limited-means-build-
     * the-boat. A target no affordable force cracks still prices
     * zero: self-limiting survives, only the cold start dies. */
    struct unit *virts[DEEP_OP_MEMBERS];
    int made = 0;
    int cap = MIN(DEEP_OP_MEMBERS, (int) ARRAY_SIZE(pool) - n_pool);

    while (made < cap) {
      double pk;

      virts[made] = unit_virtual_create(pplayer, pcity, punittype, 1);
      pool[n_pool + made] = virts[made];
      made++;
      pk = deep_take_odds_at_arrival(nmap, target, pool,
                                     n_pool + made, eta);
      if (pk >= tune->op_take_target) {
        marginal = MAX(marginal, (pk - p0) / made);
        csize = made;
        break;
      }
    }
    while (made > 0) {
      unit_virtual_destroy(virts[--made]);
    }
  }

  want = military_amortize(pplayer, pcity,
                           tune->attack_value_weight * tworth * marginal,
                           MAX(1, eta), build_cost_balanced(punittype));

  if (tune->op_take_target > 0.0 && p1 >= tune->op_take_target) {
    /* Snap-up sweep (human doctrine, T160: "build quick snap-up
     * units to steal their cities back"): one unit of the proposed
     * type takes this target alone, so the build is a price/speed
     * race, not a power race -- stock's attacker proposal is
     * power-biased. Sweep the buildable land attackers for the type
     * whose amortized want is best: cheap and fast wins on its own
     * arithmetic, no extra weighting. */
    bool overseas = tile_continent(city_tile(target)) != home;

    unit_type_iterate(ut) {
      struct unit *v2;
      double pa;
      adv_want w2;
      int rate2, eta2;

      if (ut == punittype
          || ut->attack_strength <= 0
          || utype_has_flag(ut, UTYF_CIVILIAN)
          || utype_move_type(ut) != UMT_LAND
          || !can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
        continue;
      }
      rate2 = MAX(overseas ? rate : ut->move_rate, 1);
      eta2 = (real_map_distance(city_tile(pcity), city_tile(target))
              * SINGLE_MOVE + rate2 - 1) / rate2 + (overseas ? 2 : 0);
      v2 = unit_virtual_create(pplayer, pcity, ut, 1);
      pool[n_pool] = v2;
      pa = deep_take_odds_at_arrival(nmap, target, pool, n_pool + 1,
                                     eta2);
      unit_virtual_destroy(v2);
      if (pa < tune->op_take_target) {
        continue;               /* Too light to snap it up alone. */
      }
      w2 = military_amortize(pplayer, pcity,
                             tune->attack_value_weight * tworth
                             * (pa - p0),
                             MAX(1, eta2), build_cost_balanced(ut));
      if (w2 > want) {
        want = w2;
        punittype = ut;
      }
    } unit_type_iterate_end;
  }

  /* Gate 5 (W34 A.2, E34.2b): what this player's stance charges for
   * BUYING a blade, as against gate 2's charge for committing one.
   * Applied after the snap-up sweep on purpose: a positive scalar
   * does not move an argmax, so the sweep still picks the type whose
   * own arithmetic wins the price/speed race, and the stance then
   * says how much this player wants the winner. Exactly 1.0 at
   * identity axes.
   *
   * The exemption is gate 2's own test, not a second one: reconquest
   * of what menaces us is survival rather than ambition, so a
   * barbarian holding or a hostile city on a landmass where we hold
   * cities keeps the unscaled want however deep the corner. */
  {
    double amult = deep_stance_assault_mult(pplayer);

    if (amult != 1.0 && !deep_op_target_threatens(pplayer, target)) {
      want = (adv_want) (want * amult);
      deep_plr_data(pplayer)->stance_assault_gated++;
    }
  }

  log_base(deep_metric_log_level(),
           "deep-kill turn=%d player=\"%s\" city=%d target=%d"
           " worth=" ADV_WANT_PRINTF " pool=%d p0=%.3f p1=%.3f ck=%d"
           " type=\"%s\" want=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), pcity->id, target->id,
           tworth, n_pool, p0, p1, csize, utype_rule_name(punittype),
           want);

  if (want > choice->want) {
    choice->value.utype = punittype;
    choice->want = want;
    choice->type = CT_ATTACKER;
    adv_choice_set_use(choice, "deep assault");
  }
}

struct deep_emergency {
  struct city *pcity;
  unsigned int grave_danger;
  unsigned int urgency;
  int capital;
  adv_want worth;
  int id;
};

/**********************************************************************//**
  Severity order for simultaneous city emergencies: military gravity
  first, then how close the danger is, then strategic role, then plain
  city value; city id keeps the sort deterministic. Lexicographic on
  purpose -- no scale-mixing weights to invent.
**************************************************************************/
static int deep_emergency_cmp(const void *a_, const void *b_)
{
  const struct deep_emergency *a = a_, *b = b_;

  if (a->grave_danger != b->grave_danger) {
    return a->grave_danger > b->grave_danger ? -1 : 1;
  }
  if (a->urgency != b->urgency) {
    return a->urgency > b->urgency ? -1 : 1;
  }
  if (a->capital != b->capital) {
    return b->capital - a->capital;
  }
  if (a->worth != b->worth) {
    return a->worth > b->worth ? -1 : 1;
  }
  return a->id - b->id;
}

/**********************************************************************//**
  Buy and upgrade stuff. Derived from dai_spend_gold(); the difference
  is that the disband-explorers-at-home hack is gated on exploration
  not being wanted (W06) -- stock scraps every explorer that returns
  home, which in hut games destroys the units the new explorer want
  just paid for.
**************************************************************************/
static void deep_spend_gold(struct ai_type *ait, struct player *pplayer,
                            adv_want explore_want)
{
  struct adv_choice bestchoice;
  int cached_limit = dai_gold_reserve(pplayer);
  int expenses;
  bool war_footing = dai_on_war_footing(ait, pplayer);

  if (player_get_expected_income(pplayer) < 0) {
    /* Rotation move 1 (treasury-gated spending): running at a loss,
     * raise the gold floor by the shortfall so upgrades and buys
     * cannot push the projected treasury negative -- otherwise the
     * treasury guard just sells a building to pay for the shine. */
    cached_limit -= player_get_expected_income(pplayer);
  }

  if (explore_want <= 0) {
    /* Disband explorers that are at home but don't serve a purpose. */
    city_list_iterate(pplayer->cities, pcity) {
      struct tile *ptile = pcity->tile;

      unit_list_iterate_safe(ptile->units, punit) {
        if (unit_has_type_role(punit, L_EXPLORER)
            && pcity->id == punit->homecity
            && def_ai_city_data(pcity, ait)->urgency == 0) {
          CITY_LOG(LOG_BUY, pcity, "disbanding %s to increase production",
                   unit_rule_name(punit));
          unit_do_disband_trad(pplayer, punit, ACT_REQ_PLAYER);
        }
      } unit_list_iterate_safe_end;
    } city_list_iterate_end;
  }

  dai_calc_data(pplayer, NULL, &expenses, NULL);

  do {
    bool expensive; /* Don't buy when it costs x2 unless we must */
    int buycost;
    int limit = cached_limit; /* cached_limit is our gold reserve */
    struct city *pcity = NULL;
    struct ai_city *city_data;

    /* Find highest wanted item on the buy list */
    adv_init_choice(&bestchoice);
    city_list_iterate(pplayer->cities, acity) {
      struct ai_city *acity_data = def_ai_city_data(acity, ait);

      if (acity_data->choice.want
          > bestchoice.want && ai_fuzzy(pplayer, TRUE)) {
        adv_choice_copy(&bestchoice, &(acity_data->choice));
        pcity = acity;
      }
    } city_list_iterate_end;

    /* We found nothing, so we're done */
    if (pcity == NULL) {
      break;
    }

    city_data = def_ai_city_data(pcity, ait);

    /* Not dealing with this city a second time */
    city_data->choice.want = -1;

    ADV_CHOICE_ASSERT(bestchoice);

    /* Try upgrade units at danger location (high want is usually danger) */
    if (city_data->urgency > 1) {
      if (bestchoice.type == CT_BUILDING
          && is_wonder(bestchoice.value.building)) {
        CITY_LOG(LOG_BUY, pcity, "Wonder being built in dangerous position!");
      } else {
        /* If we have urgent want, spend more */
        int upgrade_limit = limit;

        if (city_data->urgency > 1) {
          upgrade_limit = expenses;
        }
        /* Upgrade only military units now */
        dai_upgrade_units(pcity, upgrade_limit, TRUE);
      }
    }

    if (pcity->anarchy != 0 && bestchoice.type != CT_BUILDING) {
      continue; /* Nothing we can do */
    }

    /* Cost to complete production */
    buycost = city_production_buy_gold_cost(pcity);

    if (buycost <= 0) {
      continue; /* Already completed */
    }

    if (is_unit_choice_type(bestchoice.type)
        && utype_is_cityfounder(bestchoice.value.utype)) {
      if (get_city_bonus(pcity, EFT_SHRINK_FOOD) <= 0
          && bestchoice.value.utype->pop_cost > 0
          && city_size_get(pcity) <= bestchoice.value.utype->pop_cost) {
        /* Don't buy settlers in cities that cannot afford
         * the population cost. */
        continue;
      } else if (city_list_size(pplayer->cities) > 6) {
        /* Don't waste precious money buying settlers late game
         * since this raises taxes, and we want science. */
        continue;
      } else if (war_footing) {
        continue;
      }
    } else {
      /* We are not a settler. Therefore we increase the cash need we
       * balance our buy desire with to keep cash at hand for
       * emergencies and for upgrades */
      limit *= 2;
    }

    /* It costs x2 to buy something with no shields contributed */
    expensive = (pcity->shield_stock == 0)
                || (pplayer->economic.gold - buycost < limit);

    if (bestchoice.type == CT_ATTACKER
        && buycost
           > utype_build_shield_cost(pcity, NULL,
                                     bestchoice.value.utype) * 2
        && !war_footing) {
      /* Too expensive for an offensive unit */
      continue;
    }

    if (pplayer->economic.gold - expenses >= buycost
        && (!expensive
            || (city_data->grave_danger != 0
                && assess_defense(ait, pcity) == 0)
            || (bestchoice.want > 200 && city_data->urgency > 1))) {
      /* Buy stuff */
      CITY_LOG(LOG_BUY, pcity,
               "Crash buy of %s for %d (want " ADV_WANT_PRINTF ")",
               adv_choice_rule_name(&bestchoice),
               buycost,
               bestchoice.want);
      really_handle_city_buy(pplayer, pcity);
    } else if (city_data->grave_danger != 0
               && bestchoice.type == CT_DEFENDER
               && assess_defense(ait, pcity) == 0) {
      /* We have no gold but MUST have a defender */
      CITY_LOG(LOG_BUY, pcity, "must have %s but can't afford it (%d < %d)!",
               adv_choice_rule_name(&bestchoice),
               pplayer->economic.gold,
               buycost);
      try_to_sell_stuff(pplayer, pcity);
      if (pplayer->economic.gold - expenses >= buycost) {
        CITY_LOG(LOG_BUY, pcity, "now we can afford it (sold something)");
        really_handle_city_buy(pplayer, pcity);
      }
      increase_maxbuycost(pplayer, buycost);
    }
  } while (TRUE);

  if (!war_footing) {
    /* Civilian upgrades now */
    city_list_iterate(pplayer->cities, pcity) {
      dai_upgrade_units(pcity, cached_limit, FALSE);
    } city_list_iterate_end;
  }

  log_base(LOG_BUY, "%s wants to keep %d in reserve (tax factor %d)",
           player_name(pplayer), cached_limit,
           pplayer->ai_common.maxbuycost);
}

/**********************************************************************//**
  Does the useful-water gate refuse this build choice? True only for a
  ferry chosen by a city whose every sea closes on its own continent.
  Written once and asked of the winner and of its runner-up alike, so
  the recovery below cannot smuggle a hull past the gate.
**************************************************************************/
static bool deep_boat_choice_refused(struct ai_type *ait,
                                     const struct adv_choice *choice,
                                     const struct city *pcity)
{
  return deep_tuning_get()->boat_water_gate > 0
    && choice->want > 0
    && is_unit_choice_type(choice->type)
    && choice->value.utype != NULL
    && dai_is_ferry_type(choice->value.utype, ait)
    && !deep_city_water_bridges(city_tile(pcity));
}

/**********************************************************************//**
  Final build-choice selection for one city. Derived from
  dai_city_choose_build(); the differences (W01) are that the
  military-emergency gate is a want TIER check instead of a magnitude
  comparison, the military-vs-domestic merge goes through the deep
  want conversion, and the final decision is logged as a deep-want
  line for conversion calibration.
**************************************************************************/
static void deep_city_choose_build(struct ai_type *ait,
                                   struct player *pplayer,
                                   struct city *pcity)
{
  struct adv_choice *newchoice;
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  const struct civ_map *nmap = &(wld.map);
  if (has_handicap(pplayer, H_AWAY)
      && !city_built_last_turn(pcity)
      && city_data->urgency == 0) {
    /* Don't change existing productions unless we have to. */
    return;
  }

  if (is_barbarian(pplayer)) {
    dai_barbarian_choose_build(pplayer, pcity, &(city_data->choice));
  } else {
    if ((deep_want_from_choice(&city_data->choice).tier != DWT_EMERGENCY
         || city_data->urgency == 0)
        && !(dai_on_war_footing(ait, pplayer) && city_data->choice.want > 0
             && pcity->id != adv->wonder_city)) {
      /* Refuse the ferry where the bid is FORMED, not after it has
       * won. The domestic ferry rides the founder's-boat branch
       * (daidomestic.c:595), which fires only on a NEGATIVE
       * founder_want -- the sign that means "I want to found and I
       * need a boat". At a city whose every sea closes on its own
       * continent that boat can never land a passenger anywhere, so
       * the bid is withdrawn before the advisor makes it and what
       * comes back is the city's best real candidate.
       *
       * Filtering before the comparison rather than vetoing the
       * winner is the whole difference: there is no runner-up to
       * reconstruct, no second advisor call, and a POSITIVE
       * founder_want -- a settler for this city's own continent,
       * which the gate has no quarrel with -- is left alone. */
      if (deep_tuning_get()->boat_gate_at_bid > 0
          && deep_tuning_get()->boat_water_gate > 0
          && city_data->founder_want < 0
          && !deep_city_water_bridges(city_tile(pcity))) {
        int saved_founder_want = city_data->founder_want;

        log_base(deep_metric_log_level(),
                 "deep-watergate-bid turn=%d player=\"%s\" city=%d"
                 " founder_want=%d",
                 game.info.turn, player_name(pplayer), pcity->id,
                 saved_founder_want);
        city_data->founder_want = 0;
        newchoice = domestic_advisor_choose_build(ait, pplayer, pcity);
        city_data->founder_want = saved_founder_want;
      } else {
        newchoice = domestic_advisor_choose_build(ait, pplayer, pcity);
      }
      /* Re-price the caravan the domestic advisor just bid for -- or
       * bid for one it did not. Stock's trade-route producer prices a
       * caravan on an income estimate with a HARDCODED distance of ten
       * and no destination, then clamps the want to 0.1 when caravans
       * pile up; deep has picked real destinations since the trade
       * epic and never told the build queue. This is the seam where
       * the two meet: the bid has been formed and nothing has consumed
       * it yet. See deeptrade.c. */
      deep_trade_merit_apply(ait, pplayer, pcity, newchoice);
      if (deep_want_cmp(deep_want_from_choice(newchoice),
                        deep_want_from_choice(&city_data->choice)) > 0) {
        adv_choice_copy(&(city_data->choice), newchoice);
      }
      adv_free_choice(newchoice);
    }
  }

  /* The useful-water gate. A hull is only worth its shields if the
   * water it would launch into leads somewhere: if every sea this city
   * touches closes on this city's own continent, no boat built here
   * can ever put a passenger on foreign soil, at any demand, in any
   * turn.
   *
   * With boat_gate_at_bid this is a BACKSTOP and should not fire: the
   * two producers of a ferry bid -- the domestic founder's boat and
   * deep's own colony lift want -- each withdraw it at formation, so
   * a refusal here means a third producer exists that nobody has
   * accounted for. Its counter is worth reading for exactly that.
   * With boat_gate_at_bid = 0 it is the whole gate again, vetoing the
   * winner and handing the city to the fallback ladder below.
   *
   * Refusing the want rather than damping it: the claim is geometric,
   * not economic. There is no fleet size at which a lake becomes
   * useful.
   *
   * The cost is same-continent coastal hops, which this refuses too --
   * measured in the census as the closed-water hulls that carried
   * something anyway.
   *
   * boat_water_gate = 0 is the A/B switch and restores the old
   * behaviour exactly. */
  if (deep_boat_choice_refused(ait, &(city_data->choice), pcity)) {
    log_base(deep_metric_log_level(),
             "deep-watergate turn=%d player=\"%s\" city=%d type=\"%s\""
             " want=" ADV_WANT_PRINTF,
             game.info.turn, player_name(pplayer), pcity->id,
             utype_rule_name(city_data->choice.value.utype),
             city_data->choice.want);
    city_data->choice.want = 0;
  }

  /* Fallbacks */
  if (city_data->choice.want == 0) {
    /* Fallbacks do happen with techlevel 0, which is now default. -- Per */
    CITY_LOG(LOG_DEBUG, pcity, "Falling back - didn't want to build"
             " soldiers, workers, caravans, settlers, or buildings!");
    city_data->choice.want = 1;
    /* The ladder's first rung is a caravan at want 1, built exactly
     * when the city wants nothing -- which is to say, on no argument
     * at all. Under trade_merit the caravan already had its argument
     * made, by name, a few lines above: a want of 0 here means the
     * amortized value of every route reachable from this city did not
     * beat what the shields would otherwise buy. Letting the ladder
     * put one in the queue anyway would silently overrule the price.
     *
     * The rung is skipped rather than deleted: with trade_merit off
     * this is stock's ladder, unchanged, and the counter below says
     * which. */
    if (deep_tuning_get()->trade_merit > 0
        && best_role_unit(pcity, action_id_get_role(ACTION_TRADE_ROUTE))) {
      log_base(deep_metric_log_level(),
               "deep-trade-fallback turn=%d player=\"%s\" city=%d"
               " skipped=1",
               game.info.turn, player_name(pplayer), pcity->id);
    }
    if (deep_tuning_get()->trade_merit <= 0
        && best_role_unit(pcity, action_id_get_role(ACTION_TRADE_ROUTE))) {
      city_data->choice.value.utype
        = best_role_unit(pcity, action_id_get_role(ACTION_TRADE_ROUTE));
      city_data->choice.type = CT_CIVILIAN;
      adv_choice_set_use(&(city_data->choice), "fallback trade route");
    } else {
      unsigned int our_def = assess_defense_quadratic(ait, pcity);

      if (our_def == 0
          && dai_process_defender_want(ait, nmap, pplayer, pcity, 1,
                                       &(city_data->choice), 0)) {
        adv_choice_set_use(&(city_data->choice), "fallback defender");
        CITY_LOG(LOG_DEBUG, pcity, "Building fallback defender");
      } else if (best_role_unit(pcity, UTYF_WORKERS)) {
        city_data->choice.value.utype
          = dai_role_utype_for_terrain_class(pcity, UTYF_WORKERS, TC_LAND);
        city_data->choice.type = CT_CIVILIAN;
        adv_choice_set_use(&(city_data->choice), "fallback worker");
      } else {
        CITY_LOG(LOG_ERROR, pcity, "Cannot even build a fallback "
                 "(caravan/coinage/settlers). Fix the ruleset!");
        city_data->choice.want = 0;
      }
    }
  }

  if (city_data->choice.want != 0) {
    struct universal build_new;

    ADV_CHOICE_ASSERT(city_data->choice);

    deep_want_log(pcity, &city_data->choice);

    switch (city_data->choice.type) {
    case CT_CIVILIAN:
    case CT_ATTACKER:
    case CT_DEFENDER:
      build_new.kind = VUT_UTYPE;
      build_new.value.utype = city_data->choice.value.utype;
      /* The order event proper, and the only vantage that sees a hull
       * the city then BUYS: bought production is ordered and delivered
       * inside one turn and never stands in production when the
       * per-turn census runs. Measured over 20 games on isolated-150,
       * that vantage saw only 229 of 1359 hulls. */
      deep_hull_order_choice(ait, pplayer, pcity,
                             city_data->choice.value.utype);
      break;
    case CT_BUILDING:
      build_new.kind = VUT_IMPROVEMENT;
      build_new.value.building = city_data->choice.value.building;
      break;
    case CT_NONE:
      build_new.kind = VUT_NONE;
      break;
    case CT_LAST:
      build_new.kind = universals_n_invalid();
      break;
    };

    change_build_target(pplayer, pcity, &build_new,
                        E_CITY_PRODUCTION_CHANGED);
  }

}

/**********************************************************************//**
  City management. Derived from dai_manage_cities(); the difference is
  that city emergencies are collected first, ranked by severity, and
  resolved in that order, instead of in city list order where the first
  city in the list strips worked tiles and gold from more important
  ones behind it (README.AI: 'Emergencies in two cities at once aren't
  handled properly'), and that explorer production competes for the
  build choice (W06; stock has no explorer want at all).
**************************************************************************/
static void deep_manage_cities(struct ai_type *ait, struct player *pplayer)
{
  int ecount = 0, i;
  int explore_claimed = 0;
  struct deep_emergency emergencies[city_list_size(pplayer->cities) + 1];
  adv_want explore_want = deep_explore_want(pplayer, 0);

  pplayer->ai_common.maxbuycost = 0;

  TIMING_LOG(AIT_EMERGENCY, TIMER_START);
  city_list_iterate(pplayer->cities, pcity) {
    if (CITY_EMERGENCY(pcity)
        || city_granary_size(city_size_get(pcity)) == pcity->food_stock) {
      /* Having a full granary isn't an emergency, but we want to rearrange */
      auto_arrange_workers(pcity); /* This usually helps */
    }
    if (CITY_EMERGENCY(pcity)) {
      struct ai_city *city_data = def_ai_city_data(pcity, ait);

      emergencies[ecount++] = (struct deep_emergency) {
        .pcity = pcity,
        .grave_danger = city_data->grave_danger,
        .urgency = city_data->urgency,
        .capital = is_capital(pcity) ? 1 : 0,
        .worth = city_data->worth,
        .id = pcity->id,
      };
    }
  } city_list_iterate_end;

  qsort(emergencies, ecount, sizeof(emergencies[0]), deep_emergency_cmp);

  for (i = 0; i < ecount; i++) {
    log_base(deep_metric_log_level(),
             "deep-emergency turn=%d player=\"%s\" rank=%d city=%d"
             " grave=%u urgency=%u capital=%d worth=%d",
             game.info.turn, player_name(pplayer), i,
             emergencies[i].id, emergencies[i].grave_danger,
             emergencies[i].urgency, emergencies[i].capital,
             (int) emergencies[i].worth);
    resolve_city_emergency(ait, pplayer, emergencies[i].pcity);
  }

  city_list_iterate(pplayer->cities, pcity) {
    dai_city_sell_noncritical(pcity, TRUE);
    sync_cities();
  } city_list_iterate_end;
  if (pplayer->economic.tax >= 30 /* Otherwise expect it to increase tax */
      && player_get_expected_income(pplayer) < -(pplayer->economic.gold)) {
    int count = city_list_size(pplayer->cities);
    struct city *sellers[count + 1];

    /* Randomized order */
    count = 0;
    city_list_iterate(pplayer->cities, pcity) {
      sellers[count++] = pcity;
    } city_list_iterate_end;

    for (i = 0; i < count; i++) {
      int replace = fc_rand(count);
      struct city *tmp;

      tmp = sellers[i];
      sellers[i] = sellers[replace];
      sellers[replace] = tmp;
    }

    i = 0;
    while (player_get_expected_income(pplayer) < -(pplayer->economic.gold)
           && i < count) {
      dai_city_sell_noncritical(sellers[i++], FALSE);
    }
  }
  TIMING_LOG(AIT_EMERGENCY, TIMER_STOP);

  TIMING_LOG(AIT_BUILDINGS, TIMER_START);
  building_advisor(pplayer);
  deep_adjust_building_wants(ait, pplayer);
  TIMING_LOG(AIT_BUILDINGS, TIMER_STOP);

  /* Initialize the infrastructure cache, which is used shortly. */
  initialize_infrastructure_cache(pplayer);
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct adv_choice *choice;

    if (city_data->choice.want <= 0) {
      /* Note that this function mungs the seamap, but we don't care */
      TIMING_LOG(AIT_CITY_MILITARY, TIMER_START);
      choice = deep_military_choose_build(ait, &(wld.map), pplayer, pcity,
                                          NULL);
      adv_choice_copy(&(city_data->choice), choice);
      adv_free_choice(choice);
      TIMING_LOG(AIT_CITY_MILITARY, TIMER_STOP);
    }
    if (deep_expansion_blocked(ait, pplayer, pcity)
        && city_data->choice.want > 0) {
      city_data->worker_want = 0;
      city_data->founder_want = 0;
      city_data->founder_turn = game.info.turn; /* Do not consider zero we set
                                                 * here a valid value,
                                                 * if real want is needed.
                                                 * Recalculate immediately
                                                 * in such situation. */
      continue; /* Go, soldiers! */
    }
    if (explore_want > city_data->choice.want) {
      /* Stock has no explorer production want anywhere; exploration
       * only ever happens as an AIUNIT_NONE fallback (W06). */
      struct unit_type *explorer = best_role_unit(pcity, L_EXPLORER);

      if (explorer != NULL) {
        city_data->choice.value.utype = explorer;
        city_data->choice.want = explore_want;
        city_data->choice.type = CT_CIVILIAN;
        adv_choice_set_use(&city_data->choice, "explorer");
        /* This city claimed the demand; later cities in the pass see
         * it shrunk, so one global want orders one explorer at a
         * time, not a fleet. */
        explore_claimed++;
        explore_want = deep_explore_want(pplayer, explore_claimed);
      }
    }
    /* Will record its findings in pcity->worker_want */
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_START);
    contemplate_terrain_improvements(ait, pcity);
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_STOP);

    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_START);
    {
      bool fresh = FALSE;

      if (city_data->founder_turn <= game.info.turn) {
        /* Will record its findings in pcity->founder_want */
        deep_contemplate_new_city(ait, pcity);
        /* Avoid recalculating all the time.. */
        city_data->founder_turn
          = game.info.turn + fc_rand(AI_CITY_RECALC_SPEED)
            + AI_CITY_RECALC_SPEED;
        fresh = TRUE;
      } else if (pcity->server.debug) {
        /* Recalculate every turn */
        deep_contemplate_new_city(ait, pcity);
        fresh = TRUE;
      }
      if (fresh && city_data->founder_want > 0
          && deep_tuning_get()->settler_rush_weight > 0.0) {
        /* Crowded maps are a land race: whoever settles faster owns
         * the map, and the fear lattice alone would spend the race
         * turns on garrisons (human design 2026-07-30). Applied only
         * to FRESH wants so the cached value never compounds. */
        city_data->founder_want = (int)
          (city_data->founder_want
           * (1.0 + deep_tuning_get()->settler_rush_weight
                    * deep_map_crowding()));
      }
      if (fresh && city_data->founder_want > 0) {
        /* Gate 4 (W34 A.2): how much THIS player wants the land that
         * is left, as (econ/mil)^stance_expand_gain, exactly 1.0 at
         * identity axes. Whether any land remains was already asked
         * -- by the site machinery that produced this want, and by
         * the crowding term above -- so the gate prices appetite, not
         * opportunity. On the FRESH path only, for the same reason
         * the rush weight is: a want cached across recalc turns must
         * never compound. */
        double emult = deep_stance_expand_mult(pplayer);

        if (emult != 1.0) {
          city_data->founder_want
            = (int) (city_data->founder_want * emult);
          deep_plr_data(pplayer)->stance_expand_gated++;
        }
      }
    }
    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_STOP);

    /* Price a ferry choice against the water it would sail on, once
     * the choice is FINAL -- every path has spoken by here.
     *
     * Deep's own colony want is one of three sources and tuning it
     * alone could not set the fleet: stock adds a founder's boat when
     * a settler wants passage (daidomestic.c:595) and an attacker
     * ferry for a needy attacker (daimilitary.c:1687), neither of
     * which looks at idle hulls. Measured, damping the colony want
     * moved the fleet 6.14 -> 5.78 per player-turn and left it 79%
     * idle at ~12 hulls per waiting passenger.
     *
     * The first attempt hooked choose_building, which never sees this
     * at all -- that hook is handed a CT_BUILDING choice with
     * need_boat cleared (advbuilding.c:311), so the ferry test could
     * not fire once and logged zero discounts in both arms.
     *
     * Demand counts military passengers too, so an invasion short of
     * shipping is never damped. boat_idle_damping = 0 is the
     * off-switch. */
    if (city_data->choice.want > 0
        && city_data->choice.type != CT_BUILDING
        && city_data->choice.value.utype != NULL
        && deep_tuning_get()->boat_idle_damping > 0.0
        && dai_is_ferry_type(city_data->choice.value.utype, ait)) {
      int surplus = deep_water_surplus(ait, pplayer, city_tile(pcity));

      if (surplus > 0) {
        adv_want before = city_data->choice.want;

        city_data->choice.want
          = (adv_want) (before / (1.0 + deep_tuning_get()->boat_idle_damping
                                        * surplus));
        log_base(deep_metric_log_level(),
                 "deep-hull turn=%d player=\"%s\" city=%d type=\"%s\""
                 " surplus=%d want=" ADV_WANT_PRINTF " -> " ADV_WANT_PRINTF,
                 game.info.turn, player_name(pplayer), pcity->id,
                 utype_rule_name(city_data->choice.value.utype), surplus,
                 before, city_data->choice.want);
      }
    }
    ADV_CHOICE_ASSERT(city_data->choice);
  } city_list_iterate_end;
  /* Reset auto settler state for the next run. */
  dai_auto_settler_reset(ait, pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    deep_city_choose_build(ait, pplayer, pcity);
  } city_list_iterate_end;

  deep_spend_gold(ait, pplayer, explore_want);

  /* Every decision that reads building_want[] this turn has now been
   * made -- the choose loop above, the wall and wonder paths inside
   * it, and the buy pass. Hand stock's array back the way it was
   * found, so deep's credit is delivered exactly once per turn instead
   * of accumulating between stock's own recalculations. */
  deep_revert_building_wants(pplayer);

  /* Initialize for next turn -- the sentinel that stops the military
   * advisor re-running is only read at the top of the NEXT pass, so
   * it must be restored AFTER the spend pass has consumed the wants.
   * Stock resets it before dai_spend_gold, which has left the buy
   * list permanently empty since 2017 (c84ce356f5): the entire
   * crash-buy machinery, emergency defender buys included, is dead
   * code upstream. See findings/upstream-bugs.md. */
  city_list_iterate(pplayer->cities, pcity) {
    def_ai_city_data(pcity, ait)->choice.want = -1;
  } city_list_iterate_end;
}

/**********************************************************************//**
  Activities after unit movement; derived from
  dai_do_last_activities() with deep's ranked emergency handling in
  place of the stock city management.
**************************************************************************/
/**********************************************************************//**
  Never let the server force-sell a building. When the projected
  treasury cannot cover the coming turn, sell the least valuable
  sellable building ourselves -- our choice of which, one per city per
  turn like the server's own limit -- and log the emergency. The
  server's bankruptcy handler picks its victim without consulting the
  AI (cityturn.c "cant_maintain"); an autosale is a strictly worse
  version of a choice we could have made a turn earlier.
**************************************************************************/
static void deep_balance_treasury(struct ai_type *ait,
                                  struct player *pplayer)
{
  int projected = pplayer->economic.gold
                  + player_get_expected_income(pplayer);

  while (projected < 0) {
    struct city *sell_city = NULL;
    struct impr_type *sell_impr = NULL;
    double sell_score = 0.0;

    city_list_iterate(pplayer->cities, pcity) {
      if (pcity->did_sell) {
        continue;
      }
      city_built_iterate(pcity, pimprove) {
        double score;

        if (!can_city_sell_building(pcity, pimprove)) {
          continue;
        }
        /* Shed the biggest recurring bill for the smallest loss of
         * invested shields. */
        score = (1.0 + city_improvement_upkeep(pcity, pimprove))
                / (1.0 + impr_build_shield_cost(pcity, pimprove));
        if (sell_impr == NULL || score > sell_score) {
          sell_city = pcity;
          sell_impr = pimprove;
          sell_score = score;
        }
      } city_built_iterate_end;
    } city_list_iterate_end;

    if (sell_impr == NULL) {
      /* No building left to shed: disband the cheapest gold-paying
       * MILITARY unit before the server's random reaper can eat a
       * worker or settler (human rule 2026-07-31: civilians die
       * last, and never by our own hand). Prefer units in cities
       * that are not currently in danger. */
      struct unit *victim = NULL;
      int victim_cost = 0;
      bool victim_safe = FALSE;

      unit_list_iterate(pplayer->units, punit) {
        struct city *ucity = tile_city(unit_tile(punit));
        bool safe = ucity == NULL
                    || def_ai_city_data(ucity, ait)->danger == 0;
        int cost = unit_build_shield_cost_base(punit);

        if (unit_has_type_flag(punit, UTYF_CIVILIAN)
            || punit->server.upkeep_paid[O_GOLD] <= 0
            || get_transporter_occupancy(punit) > 0) {
          continue;
        }
        if (victim == NULL || (safe && !victim_safe)
            || (safe == victim_safe && cost < victim_cost)) {
          victim = punit;
          victim_cost = cost;
          victim_safe = safe;
        }
      } unit_list_iterate_end;

      if (victim != NULL) {
        log_base(deep_metric_log_level(),
                 "deep-fin turn=%d player=\"%s\" gold=%d projected=%d"
                 " disband=\"%s\"",
                 game.info.turn, player_name(pplayer),
                 pplayer->economic.gold, projected,
                 unit_rule_name(victim));
        projected += victim->server.upkeep_paid[O_GOLD];
        unit_do_disband_trad(pplayer, victim, ACT_REQ_PLAYER);
        continue;
      }
      log_base(deep_metric_log_level(),
               "deep-fin turn=%d player=\"%s\" gold=%d projected=%d"
               " nothing-sellable",
               game.info.turn, player_name(pplayer),
               pplayer->economic.gold, projected);
      return;
    }
    log_base(deep_metric_log_level(),
             "deep-fin turn=%d player=\"%s\" gold=%d projected=%d"
             " sell=\"%s\" city=%d",
             game.info.turn, player_name(pplayer),
             pplayer->economic.gold, projected,
             improvement_rule_name(sell_impr), sell_city->id);
    projected += impr_sell_gold(sell_impr)
                 + city_improvement_upkeep(sell_city, sell_impr);
    do_sell_building(pplayer, sell_city, sell_impr, "sold");
    sell_city->did_sell = TRUE;
  }
}

void deep_do_last_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  /* First, while it can still be checked: the server ran the manage
   * loop AND auto_workers_player() since the holds were taken, so
   * anything that was going to re-task a held unit has already had its
   * chance (deepops.c, deep_hold_census). */
  deep_hold_census(ait, pplayer);
  /* Before the wipe: tech_want[] is cleared here and refilled by the
   * city loop BELOW the rates, so the only honest read of what
   * research is worth to this empire is last turn's array, taken now
   * (deeprates.c). */
  deep_rates_snapshot_tech(ait, pplayer);
  dai_clear_tech_wants(ait, pplayer);

  dai_manage_government(ait, pplayer);
  dai_adjust_policies(ait, pplayer);
  TIMING_LOG(AIT_TAXES, TIMER_START);
  deep_manage_taxes(ait, pplayer);
  TIMING_LOG(AIT_TAXES, TIMER_STOP);
  TIMING_LOG(AIT_CITIES, TIMER_START);
  deep_manage_cities(ait, pplayer);
  TIMING_LOG(AIT_CITIES, TIMER_STOP);
  /* After taxes and production are settled: if the treasury still
   * cannot cover the coming turn, choose our own sacrifice before
   * the server chooses one for us. */
  deep_balance_treasury(ait, pplayer);
  TIMING_LOG(AIT_TECH, TIMER_START);
  dai_manage_tech(ait, pplayer);
  TIMING_LOG(AIT_TECH, TIMER_STOP);
  dai_manage_spaceship(pplayer);

  TIMING_LOG(AIT_ALL, TIMER_STOP);
}
