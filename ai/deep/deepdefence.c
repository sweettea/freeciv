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
#include "combat.h"
#include "effects.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "research.h"
#include "specialist.h"
#include "traits.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "cm.h"

/* ai */
#include "aitraits.h"
#include "difficulty.h"
#include "handicaps.h"

/* server */
#include "barbarian.h"
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
#include "deepdiplomacy.h"
#include "deepexplore.h"
#include "deephorizon.h"
#include "deepsite.h"
#include "deepcm.h"
#include "deepops.h"
#include "deeprates.h"
#include "deepsettler.h"
#include "deeptrunk.h"
#include "deepstance.h"
#include "deeptrade.h"
#include "deeptuning.h"
#include "deepwant.h"
#include "deepwonder.h"

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
          && deep_utype_is_sea(unit_type_get(aunit))) {
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
  int land = 0, free_land = 0, free_reach = 0;
  Continent_id ours[64];
  int nours = 0, i;

  if (dp->free_land_turn == game.info.turn) {
    return dp->free_land_permille / 1000.0;
  }

  int own_land = 0;

  /* Landmasses this player has feet on -- the same set
   * deep_explore_want() builds, and for the same reason: a founder
   * walks, so free land on a continent we do not stand on is not a
   * site until a hull exists to carry one there. */
  city_list_iterate(pplayer->cities, pcity) {
    Continent_id c = tile_continent(city_tile(pcity));

    for (i = 0; i < nours; i++) {
      if (ours[i] == c) {
        break;
      }
    }
    if (i == nours && c > 0 && nours < 64) {
      ours[nours++] = c;
    }
  } city_list_iterate_end;
  unit_list_iterate(pplayer->units, punit) {
    Continent_id c = tile_continent(unit_tile(punit));

    for (i = 0; i < nours; i++) {
      if (ours[i] == c) {
        break;
      }
    }
    if (i == nours && c > 0 && nours < 64) {
      ours[nours++] = c;
    }
  } unit_list_iterate_end;

  whole_map_iterate(&(wld.map), ptile) {
    if (!omniscient && tile_get_known(ptile, pplayer) == TILE_UNKNOWN) {
      continue;
    }
    if (is_ocean_tile(ptile)) {
      continue;
    }
    land++;
    if (tile_owner(ptile) == NULL) {
      Continent_id c = tile_continent(ptile);

      free_land++;
      for (i = 0; i < nours; i++) {
        if (ours[i] == c) {
          free_reach++;
          break;
        }
      }
    } else if (tile_owner(ptile) == pplayer) {
      own_land++;
    }
  } whole_map_iterate_end;

  dp->free_land_permille = (land > 0 ? 1000 * free_land / land : 0);
  dp->free_land_tiles = free_land;
  dp->free_land_reach = free_reach;
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

  The free-land census is a whole-map count, so on an archipelago it
  priced sites on islands this player has no way to reach, and the
  founder pool held settlers against them (expansion audit 3.8). The
  REACHABILITY law the fear lattice applies to threats applies to
  opportunities too -- deep_reachable_free_land() below.
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

  return deep_reachable_free_land(pplayer) / MAX(per_city, 1.0);
}

/**********************************************************************//**
  Unowned land a founder of this player could actually stand on: all of
  it on the landmasses we already occupy, plus the rest of the world
  only while we own something that floats a founder to it.

  No new constant: the gate is the existence of sea lift, which is
  exactly the test deep_density_fear() runs against a RIVAL's lift,
  pointed at our own. Before the first hull, overseas land is not a
  site; after it, it is, and the pool may hold settlers for it again.
**************************************************************************/
int deep_reachable_free_land(const struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  (void) deep_free_land_share(pplayer);     /* Refreshes the cache. */

  if (dp->free_land_reach >= dp->free_land_tiles) {
    return dp->free_land_tiles;             /* Nothing is overseas. */
  }
  unit_list_iterate(pplayer->units, punit) {
    const struct unit_type *ut = unit_type_get(punit);

    if (ut->transport_capacity > 0 && deep_utype_is_sea(ut)) {
      return dp->free_land_tiles;
    }
  } unit_list_iterate_end;

  return dp->free_land_reach;
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
  /* Census (knob-coverage 4a: the whole fear lattice is invisible
   * because deep_log_defence() runs before the injection pass). `raw`
   * is what the term would be with no reachability gate at all, so
   * raw - fear IS the gate's effect, and a cell where everyone is
   * reachable reports discounted=0 -- a measured no-op rather than an
   * argument. */
  double raw = 0.0;
  int n_near = 0, n_cross = 0, n_disc = 0;

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
      n_near++;
      raw += w;
      if (tile_continent(city_tile(acity))
          != tile_continent(city_tile(pcity))) {
        n_cross++;
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

            if (at->transport_capacity > 0 && deep_utype_is_sea(at)) {
              floats = 1;
              break;
            }
          } unit_list_iterate_end;
        }
        if (floats == 0) {
          w *= tune->menace_overseas_weight;
          n_disc++;
        }
      }
      fear += w;
    } city_list_iterate_end;
  } players_iterate_alive_end;

  if (n_near > 0) {
    log_base(deep_metric_log_level(),
             "deep-density turn=%d player=\"%s\" city=%d near=%d"
             " cross=%d discounted=%d raw=%.2f gated=%.2f ramp=%.2f"
             " fear=%u",
             game.info.turn, player_name(pplayer), pcity->id, n_near,
             n_cross, n_disc, raw, fear, deep_anticipation_ramp(),
             (unsigned int) (fear * deep_anticipation_ramp()));
  }

  return (unsigned int) (fear * deep_anticipation_ramp());
}

/**********************************************************************//**
  The density lattice above, aggregated per PAIR instead of per city:
  how much standing border pressure `aplayer` alone puts on all of
  `pplayer`'s cities.

  Same radius, same 1/(1+d) falloff, same reachability discount for a
  neighbour whose army cannot float -- the tunables are read, not
  re-minted, so a border-pressure number and a city's density fear can
  never disagree about what a close neighbour is. What it deliberately
  does NOT carry is the anticipation ramp or the per-city census
  counters: those belong to the fear that gets INJECTED into a city's
  danger, and this is a valuation of a relationship.

  BALANCE's consumer is the peace valuation: the pressure a war with
  this player would turn into an actual front.
**************************************************************************/
double deep_border_pressure_from(const struct player *pplayer,
                                 const struct player *aplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double fear = 0.0;
  int floats = -1;

  if (tune->density_fear_value <= 0 || pplayer == aplayer) {
    return 0.0;
  }

  city_list_iterate(aplayer->cities, acity) {
    city_list_iterate(pplayer->cities, pcity) {
      int d = real_map_distance(city_tile(acity), city_tile(pcity));
      double w;

      if (d > tune->density_fear_radius) {
        continue;
      }
      w = tune->density_fear_value / (1.0 + d);
      if (tile_continent(city_tile(acity))
          != tile_continent(city_tile(pcity))) {
        if (floats < 0) {
          floats = 0;
          unit_list_iterate(aplayer->units, aunit) {
            const struct unit_type *at = unit_type_get(aunit);

            if (at->transport_capacity > 0 && deep_utype_is_sea(at)) {
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
  } city_list_iterate_end;

  return fear;
}

/**********************************************************************//**
  The uprising horde ladder, mirrored EXACTLY from
  `server/barbarian.c:605-611` -- including the integer truncation of
  `city_max * 1.2`, because the server's `city_max` is an int too and
  a "cleaner" double version would predict a different ladder.
**************************************************************************/
static int deep_barb_uprise(int city_count)
{
  int city_max = UPRISE_CIV_SIZE;
  int uprise = 1;

  while (city_max <= city_count) {
    uprise++;
    city_max += (city_max * 1.2) + UPRISE_CIV_SIZE;
  }

  return uprise;
}

/**********************************************************************//**
  Could this city be the victim site of an uprising at all?

  `try_summon_barbarians()` throws a dart at the map, finds the NEAREST
  city to where it lands, and requires the distance to be in
  [MIN_UNREST_DIST, MAX_UNREST_DIST], a unit-free tile within one step
  of the landing spot, and (for land) that the tile is known to the
  victim. A city with no unrest annulus -- packed in among its
  neighbours, or with the whole ring occupied -- cannot be picked.

  Approximated readably, and here is exactly what the approximation
  drops: we do not check that WE are the nearest city to the candidate
  tile (a neighbour's city may be closer, in which case the neighbour
  is the victim and we are over-counting), and we ignore the sea-raider
  branch, which lands on ocean tiles. Both make this test permissive,
  never restrictive -- it can say "eligible" where the server would
  pick someone else, and it never says "safe" where the server would
  strike.
**************************************************************************/
static bool deep_barb_unrest_site(struct city *pcity)
{
  const struct civ_map *nmap = &(wld.map);
  struct player *pplayer = city_owner(pcity);
  struct tile *ctile = city_tile(pcity);

  square_iterate(nmap, ctile, MAX_UNREST_DIST, ptile) {
    if (real_map_distance(ctile, ptile) < MIN_UNREST_DIST
        || is_ocean_tile(ptile)
        || !map_is_known(ptile, pplayer)) {
      continue;
    }
    if (unit_list_size(ptile->units) == 0) {
      return TRUE;
    }
  } square_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  The shared barbarian-terror scaling, empire level. ONE function, two
  consumers (the fear term below and the garrison floor's barb class),
  because two step functions disagreeing about the same threat is how
  the derby-2 storm was missed: deep lost 107 of 207 cities in 70 turns
  to uprisings that a flat term, switched on exactly AT onset, could
  not anticipate and did not scale.

  Every factor is the server's own (`server/barbarian.c:533-613`,
  `:738-760`), mirrored rather than invented:

  1. ANTICIPATION. The server's own gate is a step at
     `game.server.onsetbarbarian`, but a garrison must EXIST at onset
     and a defender takes ~10 turns to build, so the terror ramps
     linearly over the `barb_anticipation_turns` before onset. The
     knob is the LEAD, not the shape; at 0 this reduces to the step
     function that shipped.

  2. FREQUENCY. `summon_barbarians()` runs `n * (barbarianrate - 1)`
     attempts per turn, and refuses outright for BARBS_DISABLED and
     BARBS_HUTS_ONLY. So the whole class drops out FREE in a no-barb
     cell, and `(rate - 1)` is the frequency scale -- 1 at
     BARBS_NORMAL, which is where the shipped flat value was set, so
     the standing cells see no change from this factor. Hut risk near
     cities is deepexplore's separate term and is not touched here.

  3. WILLINGNESS. `fc_rand(30) + 1 <= city_count * (rate - 1)`, i.e.
     probability MIN(1, n * (rate - 1) / 30), gated again on
     `fc_rand(100) > EFT_CIVIL_WAR_CHANCE`. The civil-war chance is
     government-dependent and ranges 20 (Federation) to 99 (Anarchy)
     in civ2civ3, so this is a real free signal and not a constant.

  4. HORDE SIZE. `barb_count = fc_rand(3) + uprise * rate`, expectation
     `1 + uprise(n) * rate` on the ladder above.

  WHY THERE IS NO PER-CITY DIVISION, though the uprising picks exactly
  ONE victim site: the dart lands uniformly on the map and the victim
  is whoever owns the nearest city, so the probability that the victim
  is US grows with our city count in the same proportion as the number
  of our cities the exposure would have to be divided among. The two
  n's cancel, and the honest per-city figure is the willingness
  probability itself -- rising with empire size and saturating at
  n = 30/(rate-1), which is the derby's "my Nth city buys barbarian
  pressure" tax priced exactly once. What survives of the site-finder
  is its ELIGIBILITY: a city with no unrest annulus cannot be the
  victim, and pays nothing (deep_barb_unrest_site()).

  Normalised to 1.0 at `barb_size_ref_cities` cities under
  BARBS_NORMAL, so the two base knobs keep the magnitude they shipped
  with instead of being silently multiplied by 30. The reference is a
  PLACEHOLDER and it, `barbarian_fear` and `floor_barb_weight` all go
  to the tuning pile together -- this commit changes the SHAPE and
  leaves the level where it was.
**************************************************************************/
static double deep_barb_ramp(void)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int lead = tune->barb_anticipation_turns;

  if (lead <= 0) {
    return (game.info.turn >= game.server.onsetbarbarian) ? 1.0 : 0.0;
  }
  if (game.info.turn >= game.server.onsetbarbarian) {
    return 1.0;
  }
  if (game.info.turn <= game.server.onsetbarbarian - lead) {
    return 0.0;
  }

  return (double) (game.info.turn
                   - (game.server.onsetbarbarian - lead)) / lead;
}

static double deep_barb_empire_scale(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int rate = game.server.barbarianrate;
  int n, ref_n, cwcv;
  double ramp, will, horde, cwc, ref_will, ref_horde;

  if (rate <= BARBS_HUTS_ONLY) {
    /* No uprisings at all in this cell: the whole class is free. */
    return 0.0;
  }

  ramp = deep_barb_ramp();
  if (ramp <= 0.0) {
    return 0.0;
  }

  n = city_list_size(pplayer->cities);
  will = MIN(1.0, n * (double) (rate - 1) / 30.0);
  horde = 1.0 + deep_barb_uprise(n) * (double) rate;
  cwcv = get_player_bonus(pplayer, EFT_CIVIL_WAR_CHANCE);
  cwc = MIN(1.0, MAX(0, cwcv) / 100.0);

  ref_n = MAX(1, tune->barb_size_ref_cities);
  ref_will = MIN(1.0, ref_n * (double) (BARBS_NORMAL - 1) / 30.0);
  ref_horde = 1.0 + deep_barb_uprise(ref_n) * (double) BARBS_NORMAL;

  return ramp * (rate - 1) * cwc * (will * horde)
         / (ref_will * ref_horde);
}

/**********************************************************************//**
  The PROBABILITY that a barbarian uprising happens this turn and that
  this city is a legal victim site for it -- a probability, not the
  terror scaling above, which folds in horde SIZE and a normalisation
  and is therefore a severity.

  Three of the four factors §3 documents, and only the three that are
  odds: the anticipation ramp, the willingness roll
  `fc_rand(30) + 1 <= n * (rate - 1)` -> MIN(1, n * (rate - 1) / 30),
  and the civil-war gate `fc_rand(100) > EFT_CIVIL_WAR_CHANCE`. Horde
  size is deliberately absent: how MANY barbarians arrive changes what
  a walk-in costs, not whether one happens, and the birth guard is
  buying the body that makes the difference between "the city changes
  hands to one warband" and "it does not".

  Permissive by construction, exactly as deep_barb_unrest_site() is:
  the per-city divisor cancels (see §"WHY THERE IS NO PER-CITY
  DIVISION" above), so this is the empire's odds attributed to an
  eligible city, which is the honest per-city figure that note names.
**************************************************************************/
static double deep_barb_walkin_odds(struct city *pcity)
{
  struct player *pplayer = city_owner(pcity);
  int rate = game.server.barbarianrate;
  double ramp;
  int n, cwcv;

  if (rate <= BARBS_HUTS_ONLY) {
    return 0.0;
  }
  ramp = deep_barb_ramp();
  if (ramp <= 0.0 || !deep_barb_unrest_site(pcity)) {
    return 0.0;
  }

  n = city_list_size(pplayer->cities);
  cwcv = get_player_bonus(pplayer, EFT_CIVIL_WAR_CHANCE);

  return ramp * MIN(1.0, n * (double) (rate - 1) / 30.0)
         * MIN(1.0, MAX(0, cwcv) / 100.0);
}

/**********************************************************************//**
  The same scaling with the site-finder's eligibility applied: a city
  the server could never pick as a victim site pays nothing.
**************************************************************************/
static double deep_barb_city_scale(struct city *pcity)
{
  double scale = deep_barb_empire_scale(city_owner(pcity));

  if (scale <= 0.0 || !deep_barb_unrest_site(pcity)) {
    return 0.0;
  }

  return scale;
}

/**********************************************************************//**
  Fear of barbarian uprisings, shaped by the server's own uprising
  formula rather than switched on flat at onset. The garrison floor
  insures otherwise-empty cities; this term keeps established cities
  from standing down the moment assessed danger returns to zero inside
  the barbarian era -- and now ramps in BEFORE the era starts, so the
  defender exists on the turn the first horde can.
**************************************************************************/
static unsigned int deep_barbarian_fear(struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();

  return (unsigned int) (tune->barbarian_fear
                         * deep_barb_city_scale(pcity));
}

/**********************************************************************//**
  One line per player per turn: the ramp, the scaling and its factors,
  and how many of this player's cities the site-finder could pick.
  This is the counter the acceptance asks for -- pre-onset ramp turns
  and the scaled-terror magnitude -- and it reports the factors apart
  so a surprising scale can be attributed rather than argued about.
**************************************************************************/
static void deep_barb_census(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double scale = deep_barb_empire_scale(pplayer);
  int n = city_list_size(pplayer->cities);
  int rate = game.server.barbarianrate;
  int sites = 0;

  if (!log_do_output_for_level(deep_metric_log_level())
      || (scale <= 0.0 && rate <= BARBS_HUTS_ONLY)) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    if (deep_barb_unrest_site(pcity)) {
      sites++;
    }
  } city_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-barb turn=%d player=\"%s\" onset=%d lead=%d rate=%d"
           " cities=%d sites=%d uprise=%d cwc=%d scale=%.4f fear=%u"
           " pre=%d",
           game.info.turn, player_name(pplayer),
           game.server.onsetbarbarian, tune->barb_anticipation_turns,
           rate, n, sites, deep_barb_uprise(n),
           get_player_bonus(pplayer, EFT_CIVIL_WAR_CHANCE), scale,
           (unsigned int) (tune->barbarian_fear * scale),
           (scale > 0.0 && game.info.turn < game.server.onsetbarbarian)
             ? 1 : 0);
}

/**********************************************************************//**
  How militarised the players we have actually met are, relative to
  us: the ratio of their summed score.units to ours, through the
  tunable slope, capped at 1.0. Unmet players contribute nothing, so a
  pre-contact empire keeps no insurance garrison at all, and the
  signal rises BEFORE any war does.

  This comment used to call score.units PUBLIC information, on the
  grounds that the historian's "mightiest civilizations" report
  broadcasts it. It does not. historian_generic() (server/report.c)
  sorts on score.units but prints only an ORDINAL RANK and a nation
  name -- never the value -- and it publishes one rotating category at
  a time, so even the ranking arrives rarely. Reading score.units
  directly is an omniscient read (RC6), like the rest of this module's
  inputs. A public-information version would take rank alone, over the
  players the report has actually named; that is W04's to build, and
  the analogy is written down here so it is not re-derived.

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
  THE ROTATION CADENCE (NETTHREAT item 3's weight). Sample, once per
  new phase, how many of this player's attack-rated units are standing
  in one of its own cities, and accumulate it over the game.

  This is the only honest source for "what fraction of its life does an
  attacker spend at home": it is measured on this player's own units in
  this player's own game, and it is exactly the quantity a build-time
  garrison credit needs. Nothing here is a constant -- the ratio starts
  undefined (no attack-rated units yet), and until it has a population
  the credit it feeds is zero.

  Deliberately UNIT-TURNS, not units: a fleet that doubles halfway
  through does not get to rewrite the first half of the record, and a
  running total over unit-turns is the same estimator the launch-rate
  guard uses on operations. Sampled at phase begin, before the turn's
  movement, so a unit that leaves this turn is counted where it slept.

  Cheap by construction: one pass over pplayer->units per player-turn,
  no combat model, no pathfinder. It runs unconditionally, because a
  counter that only exists in the arm that consumes it cannot be used
  to price the arm that does not.
**************************************************************************/
void deep_cadence_sample(struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int home = 0, total = 0;

  unit_list_iterate(pplayer->units, punit) {
    struct city *at;

    if (punit->hp <= 0
        || unit_has_type_flag(punit, UTYF_CIVILIAN)
        || adv_unit_att_rating(punit) <= 0) {
      continue;
    }
    total++;
    at = tile_city(unit_tile(punit));
    if (at != NULL && city_owner(at) == pplayer) {
      home++;
    }
  } unit_list_iterate_end;

  if (total <= 0) {
    return;
  }
  dp->cadence_home += home;
  dp->cadence_unitturns += total;

  log_base(deep_metric_log_level(),
           "deep-cadence turn=%d player=\"%s\" home=%d n=%d"
           " cum_home=%d cum_n=%d share=%.3f",
           game.info.turn, player_name(pplayer), home, total,
           dp->cadence_home, dp->cadence_unitturns,
           (double) dp->cadence_home / dp->cadence_unitturns);
}

/**********************************************************************//**
  The observed share, 0..1, or 0 while nothing has been observed yet.
  A credit multiplied by this is zero before the evidence exists,
  which is the right direction for a term that ADDS want.
**************************************************************************/
double deep_attacker_home_share(const struct player *pplayer)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);

  if (dp->cadence_unitturns <= 0) {
    return 0.0;
  }
  return (double) dp->cadence_home / dp->cadence_unitturns;
}

/**********************************************************************//**
  Can units of this class stand on water / on land? Asked of the
  ruleset's own terrain nativity, NOT of utype_move_type().

  The derived move type is unusable for this question, and the
  diagnosis first recorded here was far too narrow. It was written as
  "civ2civ3 reports the TRIREME class as UMT_LAND", i.e. one class in
  one ruleset. It is EVERY class, in EVERY ruleset, in every
  server-side caller:

    $ grep -rn "set_unit_move_type" --include=*.c .
    common/unittype.c:2961:void set_unit_move_type(struct unit_class *)
    client/packhand.c:3688:    set_unit_move_type(pclass);

  `unit_class::move_type` is assigned in exactly one place and that
  place is the CLIENT, on receipt of the ruleset packet. The server
  never calls it, so the field keeps its zero-initialised value, which
  is UMT_LAND (unittype.h:133, SPECENUM_VALUE0). A one-shot dump of
  all nine civ2civ3 classes on a resumed save read move_type=0 for
  every one of them, Sea and Air included
  (findings/venues-2026-08-12.md section 0). So server-side no
  `== UMT_SEA` test can ever be true and no `!= UMT_LAND` test can
  ever be false, and there is no ruleset in which that is not so.

  Terrain nativity cannot drift that way: the Trireme class is
  `native_to` lake and ocean in both shipped rulesets, which is what
  "it floats" means. is_native_to_class() with no extras asks exactly
  that, and it is the same accessor movement.c uses.
**************************************************************************/
static bool deep_uclass_swims(const struct unit_class *pclass)
{
  terrain_type_iterate(pterrain) {
    if (is_ocean(pterrain) && is_native_to_class(pclass, pterrain, NULL)) {
      return TRUE;
    }
  } terrain_type_iterate_end;

  return FALSE;
}

static bool deep_uclass_walks(const struct unit_class *pclass)
{
  terrain_type_iterate(pterrain) {
    if (!is_ocean(pterrain) && is_native_to_class(pclass, pterrain, NULL)) {
      return TRUE;
    }
  } terrain_type_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  The two questions the rest of deep actually asked utype_move_type(),
  answered from the same terrain nativity the two helpers above use.

  WHY THE MAPPING IS EXACTLY THIS. set_unit_move_type() derives the
  field from nativity and from nothing else: land_moving | sea_moving
  -> UMT_LAND / UMT_SEA / UMT_BOTH, and the enum has only those three
  values (unittype.h:133-137). THERE IS NO UMT_AIR: a ruleset spells
  flight as a class native to both, which is why
  deep_utype_is_airborne() below tests walks && swims. So

      UMT_LAND  ==  walks && !swims
      UMT_SEA   ==  swims && !walks
      UMT_BOTH  ==  walks &&  swims   (flight)

  and these two reproduce what the accessor WOULD have returned had
  the server ever computed it, rather than substituting a looser test.
  In particular a helicopter is neither sea nor land under either arm,
  which is what the old `== UMT_SEA` / `!= UMT_LAND` code meant and
  what its air-aware callers (the wave's flies_in arm, the carrier
  test) already assume.

  UNCONDITIONAL, and deliberately not behind a knob. The comparisons
  these replace read an enum the server never sets; there is no
  defensible "off" value for a field that is uninitialised, so there
  is nothing for a switch to select between. Thirteen call sites move
  at once and the branch is measured against its base before it
  merges -- it is not assumed correct because it is correct in
  principle.

  NOT CACHED, deliberately. is_native_to_class() with no extras is a
  single BV_ISSET, so each call is bounded by the terrain count (~14
  in civ2civ3) of trivial tests with an early exit; a per-class memo
  would need invalidation on ruleset reload and is not worth that risk
  until a profile says so.
**************************************************************************/
bool deep_utype_is_sea(const struct unit_type *ut)
{
  return (deep_uclass_swims(utype_class(ut))
          && !deep_uclass_walks(utype_class(ut)));
}

bool deep_utype_is_land(const struct unit_type *ut)
{
  return (deep_uclass_walks(utype_class(ut))
          && !deep_uclass_swims(utype_class(ut)));
}

/**********************************************************************//**
  Can a unit of this type strike a city with no visible approach --
  drop onto it, or fly to it and take it?

  Expressed from ruleset CAPABILITY, never from a named-unit list: an
  action whose result is a paradrop, or a class that is native to land
  AND ocean (which is how a ruleset says "it flies": movement.c derives
  the move type from native terrain, and there is no UMT_AIR) and that
  can conquer a city. Classic's Paratroopers match the first and its
  Helicopter the second; a ruleset that spells either capability
  differently still matches, and a ruleset with neither reports the
  class dead, which is the correct answer for that ruleset.
**************************************************************************/
bool deep_utype_is_airborne(const struct unit_type *ut)
{
  return (utype_can_do_action_result(ut, ACTRES_PARADROP)
          || utype_can_do_action_result(ut, ACTRES_PARADROP_CONQUER)
          || (deep_uclass_walks(utype_class(ut))
              && deep_uclass_swims(utype_class(ut))
              && utype_can_take_over(ut)));
}

/**********************************************************************//**
  How far such a unit reaches in one flight, in tiles: its drop range
  or its move rate, whichever is longer. The same number the garrison
  floor's airborne class is sized on, so the floor that INSURES
  against air and the survival wave that would SEE it agree by
  construction.
**************************************************************************/
int deep_utype_air_reach(const struct unit_type *ut)
{
  return MAX(ut->paratroopers_range, ut->move_rate / SINGLE_MOVE);
}

/**********************************************************************//**
  Can a unit of this type attack straight off a hull -- the "Marines"
  capability, again as a ruleset fact rather than a flag name?

  utype_can_do_action_result_when_ustate() asks the ruleset's own
  action enablers whether an attack (or a city conquest) is possible
  while the actor's unit state has a given value. A unit still aboard
  is on neither a NATIVE nor a LIVABLE tile, and both properties must
  be tested because the two rulesets shipped here key their
  ashore-first enabler on different ones: classic's and civ2civ3's
  `enabler_attack_from_native` requires UnitState OnNativeTile, while
  their `enabler_conquer_city_native` requires OnLivableTile. Testing
  only one of them reported every city-taking land unit as a marine
  from turn 1 (caught on a smoke game before it reached a batch).

  Restricted to land movers: the class this gates is "a land unit that
  can strike a coast without landing first". A ruleset whose warship
  class carries AttFromNonNative would otherwise make the escalation
  true from the first trireme, which is a different threat and is
  already priced by the boat shadow.

  No flag name appears here; a ruleset that spells the capability
  differently still matches.
**************************************************************************/
static bool deep_utype_strikes_from_hull(const struct unit_type *ut)
{
  /* A pure land unit: it walks and it cannot swim. Terrain nativity,
   * not utype_move_type() -- see deep_uclass_swims() for why the
   * derived move type cannot be trusted for this. */
  if (!deep_uclass_walks(utype_class(ut))
      || deep_uclass_swims(utype_class(ut))) {
    return FALSE;
  }
  return ((utype_can_do_action_result_when_ustate(ut, ACTRES_ATTACK,
                                                  USP_NATIVE_TILE, FALSE)
           && utype_can_do_action_result_when_ustate(ut, ACTRES_ATTACK,
                                                     USP_LIVABLE_TILE, FALSE))
          || (utype_can_do_action_result_when_ustate(ut, ACTRES_CONQUER_CITY,
                                                     USP_NATIVE_TILE, FALSE)
              && utype_can_do_action_result_when_ustate(ut,
                                                        ACTRES_CONQUER_CITY,
                                                        USP_LIVABLE_TILE,
                                                        FALSE)));
}

/**********************************************************************//**
  Does this type float, and carry cargo of the kind we are afraid of?

  `flies` selects which cargo matters. A LANDING force is cargo that
  walks and cannot swim -- a pure land unit, which must be put ashore.
  A mobile AIRBASE carries cargo that both walks and swims, which is
  how a ruleset spells flight (there is no UMT_AIR). A warship that
  carries nothing is neither: it is a bombardment threat, and the boat
  shadow already prices that.
**************************************************************************/
static bool deep_utype_floats(const struct unit_type *ut, bool flies)
{
  if (ut->transport_capacity <= 0 || !deep_uclass_swims(utype_class(ut))) {
    return FALSE;
  }
  unit_class_iterate(pclass) {
    if (!BV_ISSET(ut->cargo, uclass_index(pclass))) {
      continue;
    }
    if (flies ? (deep_uclass_walks(pclass) && deep_uclass_swims(pclass))
              : (deep_uclass_walks(pclass) && !deep_uclass_swims(pclass))) {
      return TRUE;
    }
  } unit_class_iterate_end;

  return FALSE;
}

static bool deep_utype_floats_landing(const struct unit_type *ut)
{
  return deep_utype_floats(ut, FALSE);
}

static bool deep_utype_is_carrier(const struct unit_type *ut)
{
  return deep_utype_floats(ut, TRUE);
}

/**********************************************************************//**
  Which of the threat classes the danger assessment cannot see are
  actually LIVE for this player this turn (human direction 2026-08-05).

  Every test here is a server or ruleset FACT and therefore free: the
  server's own barbarian gate, whether a met non-allied rival owns a
  hull that floats, whether its known tech lets it build something that
  flies or something that attacks off a hull, and whether it holds a
  city on a landmass we stand on. Nothing in this function is tunable,
  so none of it can be mis-set; the weights the classes carry are the
  tunables, and they live at the consumer.

  "Met non-allied" rather than "at war" is deliberate and it is the
  one-owner rule doing the work. The at-war cases are already priced by
  the fear terms above -- embarked cargo (F1), the boat shadow (F2),
  neighbouring cities (F3), the barbarian era (F5) -- and every one of
  those makes danger nonzero, which makes the floor ineligible in that
  city. So the floor can only ever speak about the rival that COULD go
  to war and has not, which is exactly the risk an assessment of
  present danger is blind to.

  Same-continent rivals with boats count toward the sea class: a
  landing is not only trans-oceanic, and a neighbour can coastal-hop
  around a defended border. The test is "floats transports", not
  "floats transports and lives elsewhere".
**************************************************************************/
static const struct deep_floor_class *
deep_floor_class_facts(struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  struct deep_floor_class *fc = &dp->floor_class;
  Continent_id ours[64];
  int n_ours = 0;

  if (fc->turn == game.info.turn && game.info.turn > 0) {
    return fc;
  }
  memset(fc, 0, sizeof(*fc));
  fc->turn = game.info.turn;

  /* The server's own gate, the same one barbarian.c uses. */
  /* Not a step at onset any more: the shared scaling opens the class
   * during the anticipation ramp and closes it whenever the server
   * could not raise a horde at all (BARBS_DISABLED, HUTS_ONLY). The
   * WEIGHT the class then carries is per-city and lives at the
   * consumer, deep_floor_weigh(). */
  fc->barb = (deep_barb_empire_scale(pplayer) > 0.0);

  city_list_iterate(pplayer->cities, pcity) {
    Continent_id c = tile_continent(city_tile(pcity));
    int i;

    for (i = 0; i < n_ours; i++) {
      if (ours[i] == c) {
        break;
      }
    }
    if (i == n_ours && c > 0 && n_ours < 64) {
      ours[n_ours++] = c;
    }
  } city_list_iterate_end;

  players_iterate_alive(aplayer) {
    /* Barbarians and animals are excluded here, and that is the
     * one-owner rule again: the barbarian class owns them through the
     * server's own onset gate, and their nations are exempted from the
     * ruleset's conquest enablers anyway. Left in, civ2civ3's animal
     * player -- which starts met and can "build" anything -- reported
     * the marines escalation live from turn 4 of every game. */
    if (aplayer == pplayer
        || is_barbarian(aplayer)
        || pplayers_allied(pplayer, aplayer)
        || player_diplstate_get(pplayer, aplayer)->type == DS_NO_CONTACT) {
      continue;
    }

    if (!fc->sea) {
      unit_list_iterate(aplayer->units, aunit) {
        if (deep_utype_floats_landing(unit_type_get(aunit))) {
          fc->sea = TRUE;
          break;
        }
      } unit_list_iterate_end;
    }

    if (!fc->border) {
      city_list_iterate(aplayer->cities, acity) {
        Continent_id c = tile_continent(city_tile(acity));

        for (int i = 0; i < n_ours; i++) {
          if (ours[i] == c) {
            fc->border = TRUE;
            break;
          }
        }
        if (fc->border) {
          break;
        }
      } city_list_iterate_end;
    }

    /* Tech-gated capabilities: what this rival's KNOWN tech would let
     * it build, not what it happens to own. A class whose first unit
     * is one turn from the shipyard is live; the point of insurance is
     * that it precedes the event. */
    unit_type_iterate(ut) {
      if (!can_player_build_unit_direct(aplayer, ut, RPT_CERTAIN, FALSE)) {
        continue;
      }
      if (deep_utype_is_airborne(ut)) {
        int reach = deep_utype_air_reach(ut);

        fc->air = TRUE;
        fc->air_range = MAX(fc->air_range, reach);
      }
      if (!fc->marines && deep_utype_strikes_from_hull(ut)) {
        fc->marines = TRUE;
      }
      if (!fc->carrier && deep_utype_is_carrier(ut)) {
        fc->carrier = TRUE;
      }
    } unit_type_iterate_end;
  } players_iterate_alive_end;

  return fc;
}

/* What each live class is worth to THIS city, and the weight the
 * floor takes from them. */
struct deep_floor_weights {
  double barb, sea, air, border, best;
};

/**********************************************************************//**
  Is there water that LEADS ANYWHERE within `range` tiles of this tile?

  The carrier refinement's geometry (human, 2026-08-05): land-based air
  flies from a rival's cities and airbases and is range-limited by
  them, but a carrier makes the effective airbase any offshore water,
  so the reachable set becomes "everything within air range of open
  water". Reuses the water-gate machinery -- deep_water_bridges() is
  get_lake_surrounder() -- so a closed lake is not an airbase, exactly
  as it is not a landing route.
**************************************************************************/
static bool deep_open_water_within(const struct civ_map *nmap,
                                   const struct tile *ptile, int range)
{
  square_iterate(nmap, ptile, MAX(range, 1), atile) {
    if (deep_water_bridges(tile_continent(atile))) {
      return TRUE;
    }
  } square_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  What each live threat class is worth to this particular city.

  The classes are player-level facts; the WEIGHTS are per-city, which
  is the whole point of the refinements (human, 2026-08-05): a
  lake-locked interior city does not carry a landing threat the way a
  port does, a deep-interior city is out of air range, and a border
  city facing a land neighbour carries more than one facing an ocean.

  Three geometry rules, all reusing machinery that already exists:

  - **Sea.** Exterior = coastal AND on water that leads somewhere
    (deep_city_water_bridges, i.e. get_lake_surrounder). Interior
    cities carry floor_interior_weight of the class. Marines escalate:
    before attack-from-hull tech exists anywhere a landing must
    disembark first and there is a turn of visible warning, so the
    class is mild; after it, the exterior weight is multiplied and the
    interior weight divided by the same knob, which sharpens the split
    rather than merely raising it.

  - **Air.** Land-based air reaches a city within air_range of a rival
    CITY. Carrier-capable air reaches any city within air_range of
    water that leads anywhere. Deep-interior cities stay out of reach
    in both cases; that is the class's own geometry and it is free.

  - **Border.** Proximity to the nearest met non-allied city ON THIS
    CITY'S LANDMASS, shaped R/(R+d) with R = density_fear_radius, so
    the shape reuses the radius the density fear already uses and adds
    one weight rather than a distance scale of its own. A rival with no
    city on our landmass contributes nothing here, so an overseas-only
    rival cannot outweigh a land neighbour for a border city.

  ONE OWNER, and it is structural, not a convention. F3 (density fear)
  prices exactly this proximity inside its radius -- and whenever F3 is
  nonzero the city's danger is nonzero and the floor is not eligible at
  all. So F3 owns the near field and the floor's border class owns only
  the far field where F3 has truncated to zero. The same argument
  retires the F5/barbarian and F2/boat-shadow double-charge questions:
  when either fires, the floor is silent in that city.
**************************************************************************/
static void deep_floor_weigh(const struct civ_map *nmap, struct city *pcity,
                             const struct deep_floor_class *fc,
                             struct deep_floor_weights *w)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  struct tile *ptile = city_tile(pcity);
  Continent_id home = tile_continent(ptile);
  int near_same = -1, near_any = -1;

  memset(w, 0, sizeof(*w));

  if (fc->barb) {
    /* Base x the shared scaling, the same scaling the fear term
     * uses, so the floor and the fear cannot disagree about how
     * frightening this era is for THIS empire at THIS size. */
    w->barb = tune->floor_barb_weight * deep_barb_city_scale(pcity);
  }

  if (fc->sea) {
    bool exterior = (is_terrain_class_near_tile(nmap, ptile, TC_OCEAN)
                     && deep_city_water_bridges(ptile));
    double esc = MAX(tune->floor_marines_mult, 1.0);

    /* The escalation moves BOTH weights and it moves them DOWNWARD
     * from full, never upward past it. Before attack-from-hull tech
     * exists anywhere a landing must disembark first, so a port gets
     * a turn of visible warning and the class is mild (1/esc); after
     * it, a port can be struck off the hull with no warning and the
     * class reaches full weight while the interior weight drops by
     * the same factor. The split sharpens either way -- 2.7x to 6x at
     * the placeholder values -- and the ceiling stays at
     * floor_sea_weight, so no live class can ever buy MORE floor than
     * deep applied unconditionally before it was gated. */
    if (fc->marines) {
      w->sea = tune->floor_sea_weight
               * (exterior ? 1.0 : tune->floor_interior_weight / esc);
    } else {
      w->sea = tune->floor_sea_weight
               * (exterior ? 1.0 / esc : tune->floor_interior_weight);
    }
  }

  if (fc->air || fc->border) {
    players_iterate_alive(aplayer) {
      if (aplayer == pplayer
          || pplayers_allied(pplayer, aplayer)
          || player_diplstate_get(pplayer, aplayer)->type == DS_NO_CONTACT) {
        continue;
      }
      city_list_iterate(aplayer->cities, acity) {
        int d = real_map_distance(city_tile(acity), ptile);

        if (near_any < 0 || d < near_any) {
          near_any = d;
        }
        if (tile_continent(city_tile(acity)) == home
            && (near_same < 0 || d < near_same)) {
          near_same = d;
        }
      } city_list_iterate_end;
    } players_iterate_alive_end;
  }

  if (fc->border && near_same >= 0) {
    w->border = tune->floor_border_weight
                * (double) tune->density_fear_radius
                  / (tune->density_fear_radius + near_same);
  }

  if (fc->air
      && (fc->carrier
          ? deep_open_water_within(nmap, ptile, fc->air_range)
          : (near_any >= 0 && near_any <= fc->air_range))) {
    w->air = tune->floor_air_weight;
  }

  /* MAX, not sum: the floor buys a token garrison, and a second blind
   * spot does not buy a second token. The classes decide whether to
   * insure and how firmly, not how many units to insure with. */
  w->best = MAX(MAX(w->barb, w->sea), MAX(w->air, w->border));
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

  Census (`deep-floor`, one line per ELIGIBLE city-turn from the
  once-per-turn caller only): the floor owns most of deep's standing
  army by the E34.2/E34.2b measurement -- 73-74% of military
  city-turns sit at raw danger zero -- and had no counter at all, so
  neither its size nor an off-switch could be proven. `applied` is the
  danger actually injected, so an arm with the floor switched off
  reports eligible city-turns with applied=0 and the switch is proven
  rather than assumed.

  Eligibility is a RESIDUAL: every anticipatory fear term above has
  already been added, so the floor can only fire in a city where F1-F6
  all came back zero (and satisfaction cannot have shrunk them --
  `deep_fear_satisfaction` returns 1.0 while the garrison is below the
  floor, which is exactly when the floor is eligible). One owner per
  threat class therefore holds by construction: whenever a fear term
  prices a class, the floor is silent in that city.

  CLASS-GATED (human direction, 2026-08-05). Insurance against what the
  assessment cannot see is worth nothing while nothing it insures
  against can happen. The floor is now the weight of the strongest LIVE
  blind-spot class for this city, and zero when no class is live:

    early game, no live class and no active threat
      => no floor defender instead of a settler or a building.

  "Active threat" needs no new test -- it is the danger == 0
  precondition this function already had. At the placeholder weights a
  city with any class live gets exactly the floor deep applied
  unconditionally before, so the change is a pure gate at shipping
  values.
**************************************************************************/
static void deep_apply_garrison_floor(const struct civ_map *nmap,
                                      struct city *pcity,
                                      struct ai_city *city_data,
                                      double upkeep_discount,
                                      double menace, bool log_it)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct deep_floor_class *fc;
  struct deep_floor_weights w;
  double fmult;
  unsigned int applied;

  if (city_data->danger != 0
      || unit_list_size(city_tile(pcity)->units) >= tune->garrison_floor) {
    return;
  }
  fc = deep_floor_class_facts(city_owner(pcity));
  deep_floor_weigh(nmap, pcity, fc, &w);
  /* Gate 6 (W34, human 2026-08-05): the personality multiplier. Gate
   * 1 refuses to scale the floor because it scales anticipation TOWARD
   * the floor; gate 6 asks the different question of how much
   * insurance THIS personality buys against blind spots. Exactly 1.0
   * at identity axes, so the benchmark baseline is a uniform floor. */
  fmult = deep_stance_floor_mult(city_owner(pcity));
  if (fmult != 1.0 && w.best > 0.0) {
    deep_plr_data(city_owner(pcity))->stance_floor_gated++;
  }
  applied = (unsigned int) (tune->garrison_floor_danger * upkeep_discount
                            * menace * w.best * fmult);
  city_data->danger = applied;
  if (log_it) {
    log_base(deep_metric_log_level(),
             "deep-floor turn=%d player=\"%s\" city=%d units=%d"
             " menace=%.3f updisc=%.3f barb=%d sea=%d air=%d border=%d"
             " mar=%d carrier=%d airrange=%d wbarb=%.3f wsea=%.3f"
             " wair=%.3f wborder=%.3f w=%.3f fmult=%.3f applied=%u",
             game.info.turn, player_name(city_owner(pcity)), pcity->id,
             unit_list_size(city_tile(pcity)->units), menace,
             upkeep_discount, fc->barb, fc->sea, fc->air, fc->border,
             fc->marines, fc->carrier, fc->air_range, w.barb, w.sea,
             w.air, w.border, w.best, fmult, applied);
  }
}

/**********************************************************************//**
  Sequential-attrition deficit (combat workstream): the linear danger
  sum sits at parity when the garrison is rated equal to the wave, but
  surviving the wave means winning every duel in it, and that
  compounds down (deepodds.c). When the computed survival odds fall
  short of the target, the shortfall funds extra defence against the
  wave deepcombat.c assembles from aplayer->units with no visibility
  test (an omniscient read, RC6; findings/omniscience-census-2026-08-
  11.md S5), not from units actually observed; the anticipatory fear
  terms keep covering threats that have not yet materialized into
  that wave. log_it is set by the once-per-turn caller so the
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
    unsigned int barb = deep_barbarian_fear(pcity);
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
    /* NOT scaled: the survival deficit is the floor gate 1 scales
     * anticipation TOWARD, not below -- same as the garrison floor
     * below. The wave itself is an omniscient read of aplayer->units
     * with no visibility test (RC6), not an OBSERVED one; see
     * findings/omniscience-census-2026-08-11.md S5. */
    city_data->danger
      += deep_survival_deficit_fear(&(wld.map), pcity, updisc, TRUE);
    credit = deep_incoming_defence_credit(ait, pcity)
             + deep_muster_reserve_credit(ait, pcity);
    city_data->danger -= MIN(city_data->danger, credit);
    deep_apply_garrison_floor(&(wld.map), pcity, city_data, updisc,
                              menace, TRUE);
  } city_list_iterate_end;
}

/**********************************************************************//**
  The DEFENSIVE tech wall: sibling of deephorizon.c's H9.

  H9 found that a city whose growth is capped by a missing advance
  contributed ZERO pressure toward exactly the advance that would solve
  its problem -- the horizon stopped at the tech wall -- and routed the
  capacity credit into tech_want[] for the gating advance instead. The
  military side has the identical hole and it is worse, because losing
  a city is not recoverable by waiting: a city that cannot hold prices
  its DEFICIT as fear, the fear buys the best defender it can build,
  and if the best defender it can BUILD loses to the wave while a
  defender it could RESEARCH would not, nothing in deep or in stock
  ever notices the difference. Deep's tech want is otherwise assembled
  from building effects and stock's per-unit-type want, neither of
  which knows a city is losing.

  The scan is the eta-reinforcement model's, run twice: the best
  defence strength buildable HERE NOW, and the best buildable LATER
  (can_city_build_unit_later -- the ruleset's reachable set, not the
  world's). A strictly better later defender means the wall is real,
  and the gating advance is the unknown VUT_ADVANCE requirement of that
  unit type. Ties and equalities route nothing: the want exists only
  where research would change the outcome.

  The credit is the same quantity the fear is: survival_fear_value x
  the normalised deficit, times defensive_techwall_weight. That makes
  the knob a pure ratio -- "a turn of research is worth this many
  turns of the fear it would retire" -- rather than a magnitude in a
  new unit.

  Deliberately NOT stance-gated, and for H9's own stated reason: what
  leaves the build queue and lands in tech_want[] is tech DIRECTION,
  which is W34 A.3's gate to build, and scaling it here would put two
  gates on one quantity.
**************************************************************************/
static int deep_techwall_routed = 0;
static double deep_techwall_paid = 0.0;

static void deep_defensive_techwall(struct ai_type *ait,
                                    struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  const struct research *presearch = research_get(pplayer);

  if (tune->defensive_techwall_weight <= 0.0
      || tune->survival_target <= 0.0) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    const struct unit_type *want_type = NULL;
    double surv, deficit, credit;
    int wave = 0, best_now = 0, best_later = 0;

    surv = deep_city_survival_verdict(nmap, pcity, tune->survival_horizon,
                                      &wave);
    if (wave == 0 || surv >= tune->survival_target) {
      continue;
    }

    unit_type_iterate(ut) {
      if (ut->defense_strength <= 0
          || !deep_utype_is_land(ut)) {
        continue;
      }
      if (can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
        best_now = MAX(best_now, ut->defense_strength);
      } else if (can_city_build_unit_later(nmap, pcity, ut)
                 && ut->defense_strength > best_later) {
        best_later = ut->defense_strength;
        want_type = ut;
      }
    } unit_type_iterate_end;

    if (want_type == NULL || best_later <= best_now) {
      continue;               /* No wall, or nothing behind it. */
    }

    deficit = (tune->survival_target - surv) / tune->survival_target;
    credit = tune->survival_fear_value * deficit
             * tune->defensive_techwall_weight;
    if (credit <= 0.0) {
      continue;
    }

    requirement_vector_iterate(&want_type->build_reqs, preq) {
      if (VUT_ADVANCE == preq->source.kind && preq->present
          && research_invention_state(presearch,
               advance_number(preq->source.value.advance))
             != TECH_KNOWN) {
        def_ai_player_data(pplayer, ait)->tech_want[
            advance_index(preq->source.value.advance)] += credit;
        deep_techwall_routed++;
        deep_techwall_paid += credit;
        log_base(deep_metric_log_level(),
                 "deep-defwall turn=%d player=\"%s\" city=%d p=%.3f"
                 " wave=%d now=%d later=%d unit=\"%s\" tech=\"%s\""
                 " credit=%.1f",
                 game.info.turn, player_name(pplayer), pcity->id, surv,
                 wave, best_now, best_later, utype_rule_name(want_type),
                 advance_rule_name(preq->source.value.advance), credit);
      }
    } requirement_vector_iterate_end;
  } city_list_iterate_end;

  if (deep_techwall_routed > 0) {
    log_base(deep_metric_log_level(),
             "deep-defwall-sum turn=%d player=\"%s\" routed=%d paid=%.1f",
             game.info.turn, player_name(pplayer), deep_techwall_routed,
             deep_techwall_paid);
  }
  deep_techwall_routed = 0;
  deep_techwall_paid = 0.0;
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
      if (deep_op_claiming(pplayer, punit->id) >= 0) {
        /* An operation's claim outranks the rotation: an obsolete
         * Phalanx drafted from the walls and mustering in a quiet
         * city was meltable, which disbands a unit another intention
         * is counting on (military audit C3). Not counted in `count`
         * either -- it is not this garrison's to spend. */
        log_base(deep_metric_log_level(),
                 "deep-claimskip turn=%d player=\"%s\" pass=melt"
                 " unit=%d city=%d op=%d",
                 game.info.turn, player_name(pplayer), punit->id,
                 pcity->id, deep_op_claiming(pplayer, punit->id));
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
    {
      double p_without
        = tune->survival_target > 0.0
          ? deep_city_stack_survival_without(nmap, pcity,
                                             tune->survival_horizon,
                                             worst, NULL)
          : 1.0;

      /* The question every other leave-the-city decision asks, and
       * the melt did not (military audit C3). Its own gate is
       * danger == 0 && urgency == 0 -- presence counts, and with
       * satisfied_fear_discount at its default a SATISFIED city reads
       * danger 0 with an observed wave standing outside it. Losing a
       * body is losing a body whatever the fear lattice says. */
      if (p_without < tune->survival_target) {
        log_base(deep_metric_log_level(),
                 "deep-meltblock turn=%d player=\"%s\" city=%d"
                 " unit=\"%s\" p=%.3f target=%.3f",
                 game.info.turn, player_name(pplayer), pcity->id,
                 unit_rule_name(worst), p_without,
                 tune->survival_target);
        continue;
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
        || !deep_utype_is_sea(bt)) {
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
            && deep_utype_is_sea(wt)
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

  THE PRIZE (M4b item 1). Both of those gates price the RISK. Neither
  prices the TARGET, which is the asymmetry the operations layer does
  not have: an op values the conquest it buys, and the sortie valued
  nothing at all, so a lone Warriors and a nine-unit siege stack were
  the same decision at the same odds. Under killstack a won duel on an
  open tile annihilates the whole stack -- is_stack_vulnerable() is
  FALSE only inside a city and on EF_NO_STACK_DEATH terrain -- so the
  prize is the summed shield value of everything standing there.

  With sortie_prize_gain positive, selection is by

      p x prize x gain - (1 - p) x our own build cost

  rather than by p alone, and that score must be positive as well as
  clearing both gates. At 0 the pass is exactly what it was.
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
    double best_prize = 0.0, best_score = 0.0;
    bool priced = (tune->sortie_prize_gain > 0.0);
    struct deep_cover_move cover;
    bool covered = FALSE;

    unit_list_iterate(city_tile(pcity)->units, u) {
      if (unit_owner(u) != pplayer
          || unit_has_type_flag(u, UTYF_CIVILIAN)
          || u->hp <= 0
          || u->moves_left < SINGLE_MOVE
          || def_ai_unit_data(u, ait)->done
          || adv_unit_att_rating(u) <= 0) {
        continue;
      }
      if (deep_op_claiming(pplayer, u->id) >= 0) {
        /* Every other unit-claiming pool asks this first; the two
         * sortie passes were the only ones that did not (military
         * audit C2), so a mustering or marching op member could be
         * spent on a duel outside the walls of the city it was
         * passing through. */
        log_base(deep_metric_log_level(),
                 "deep-claimskip turn=%d player=\"%s\" pass=kill"
                 " unit=%d city=%d op=%d",
                 game.info.turn, player_name(pplayer), u->id, pcity->id,
                 deep_op_claiming(pplayer, u->id));
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
        if (!priced) {
          if (chance > best) {
            best = chance;
            blade = u;
            victim = pdef;
          }
        } else {
          double prize = 0.0, score;

          if (is_stack_vulnerable(atile)) {
            /* Killstack: the duel is for the whole tile. Everything
             * standing on it dies, ours included if any -- but a tile
             * with our own units on it is not one we are sortieing
             * against, and the at-war test above already picked the
             * defender out of an enemy stack. */
            unit_list_iterate(atile->units, tu) {
              prize += unit_build_shield_cost_base(tu);
            } unit_list_iterate_end;
          } else {
            prize = unit_build_shield_cost_base(pdef);
          }
          score = chance * prize * tune->sortie_prize_gain
                  - (1.0 - chance) * unit_build_shield_cost_base(u);
          if (blade == NULL || score > best_score) {
            best = chance;
            best_prize = prize;
            best_score = score;
            blade = u;
            victim = pdef;
          }
        }
      } adjc_iterate_end;
    } unit_list_iterate_end;

    if (blade == NULL) {
      continue;
    }
    if (best < tune->sortie_win_floor
        || (priced && best_score <= 0.0)) {
      /* Declined, WITH the prize it declined -- the counter the item
       * owes: a pass that turns down big stacks at 0.84 is a
       * different finding from one that never sees a big stack. */
      if (priced) {
        log_base(deep_metric_log_level(),
                 "deep-sortie-no turn=%d player=\"%s\" city=%d"
                 " kill=\"%s\" victim=\"%s\" p=%.3f prize=%.1f"
                 " score=%.1f",
                 game.info.turn, player_name(pplayer), pcity->id,
                 unit_rule_name(blade), unit_rule_name(victim), best,
                 best_prize, best_score);
      }
      continue;
    }
    /* THE COST SIDE, priced after rearrangement (NETTHREAT item 4).
     * The shipped gate asks whether the city holds with a hole in it;
     * with cover_removal on it asks whether the city holds once the
     * cheapest neighbour has backfilled, which is the night that
     * actually happens. The lender is guaranteed to stay above the
     * floor by deep_city_sparable_blade()'s own must_stay_safe test,
     * so this cannot rob one city to save another. */
    covered = deep_cover_removal_cost(ait, nmap, pcity, blade, &cover);
    if ((covered
         ? cover.surv_after
         : deep_city_stack_survival_without(nmap, pcity,
                                            tune->survival_horizon,
                                            blade, NULL))
        < tune->survival_target) {
      continue;
    }
    log_base(deep_metric_log_level(),
             "deep-sortie turn=%d player=\"%s\" city=%d kill=\"%s\""
             " victim=\"%s\" p=%.3f prize=%.1f score=%.1f",
             game.info.turn, player_name(pplayer), pcity->id,
             unit_rule_name(blade), unit_rule_name(victim), best,
             best_prize, best_score);
    /* The hole this sortie opens is covered by whoever can cover it
     * cheapest, and ORDERED rather than merely priced -- the removal
     * is committed on this line, so the backfill is committed with it
     * (NETTHREAT item 4). No-op when the gate above was the shipped
     * one, because `covered` is then FALSE. */
    if (covered) {
      deep_cover_order(ait, pcity, &cover);
    }
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
      if (deep_op_claiming(pplayer, g->id) >= 0) {
        /* C2 again, and the sharper half: a MARCH member walks under
         * stock's leash and is not held, so without this check it can
         * be handed a worker to escort in the middle of a crossing. */
        log_base(deep_metric_log_level(),
                 "deep-claimskip turn=%d player=\"%s\" pass=cover"
                 " unit=%d city=%d op=%d",
                 game.info.turn, player_name(pplayer), g->id, pcity->id,
                 deep_op_claiming(pplayer, g->id));
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
      /* Nothing to duel: get_defender() above has no visibility test
       * at all (an omniscient read, RC6; findings/omniscience-census-
       * 2026-08-11.md S5), so "nothing" means the tile is genuinely
       * empty or its occupant is not at war with us, never merely
       * unseen. A walk-in capture needs no rest, and a stale mission
       * is the manager's to re-decide. */
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
        || !deep_utype_is_land(unit_type_get(g))
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

/* The hole-covering price's census, per player per turn: removals
 * priced, removals where a cover genuinely existed, and the survival
 * the covers bought back. Zero lines with cover_removal = 0, because
 * the pricing function returns before its first read. */
static int deep_cover_priced = 0, deep_cover_found = 0;
static double deep_cover_saved = 0.0;

/**********************************************************************//**
  HOLES GET COVERED (NETTHREAT item 4, human 2026-08-12): price a
  defender's removal AFTER the cheapest single covering move.

  Every consumer that takes a blade out of a city -- the kill sortie,
  the operations layer's garrison draft, and MUSTER's home-defence
  abandon trigger when it lands -- prices the removal as the LOCAL
  HOLE: surv(city) minus surv(city without this blade), as if the rest
  of the lattice were nailed to the map. It is not. If a neighbour has
  a blade it can genuinely spare, the hole closes behind the departure
  and the honest cost is the post-rearrangement marginal loss, which
  is smaller -- so the naive price refuses sorties and drafts that the
  empire could actually afford.

  ONE STEP, and no new optimizer (the brief is explicit). The
  candidates are own cities on this continent whose sparable blade can
  ARRIVE within survival_horizon -- deep_defend_counter_claims()'s own
  admission test, on the cover's own move rate -- and the blade each of
  them could lend is deep_city_sparable_blade(..., must_stay_safe =
  TRUE), the same rotation/rehoming selector the counter-claim and the
  relay already ask. The multi-hop generalisation is
  deep_relay_chain(); it exists, it is not called here, and that is
  the approximation.

  THE PRICE IS EMPIRE-WIDE, which is the point of pricing it at all:

      cost = [surv(A) - surv(A without blade, with cover)]   the hole left
           + [surv(B) - surv(B without cover)]               the lender's

  minimised over covers. The lender's loss is charged in full rather
  than waved through on the must_stay_safe gate: "still above the
  floor" is not "free", and a rearrangement that costs more than the
  hole it fills is one this function must be able to reject. When no
  candidate beats the naive removal, `cover` comes back NULL and
  `cost` is exactly the local hole -- so a consumer reading `cost`
  gets the old number where the lattice really is rigid.

  Returns FALSE and touches nothing when cover_removal = 0, so every
  consumer keeps its shipped arithmetic in the off arm.
**************************************************************************/
bool deep_cover_removal_cost(struct ai_type *ait, const struct civ_map *nmap,
                             struct city *pcity, struct unit *blade,
                             struct deep_cover_move *out)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  double base, naive;

  if (tune->cover_removal <= 0 || blade == NULL || out == NULL) {
    return FALSE;
  }

  base = deep_city_stack_survival(nmap, pcity, tune->survival_horizon, NULL);
  naive = deep_city_stack_survival_without(nmap, pcity,
                                           tune->survival_horizon, blade,
                                           NULL);
  out->cover = NULL;
  out->from = NULL;
  out->naive_cost = base - naive;
  out->cost = out->naive_cost;
  out->surv_after = naive;

  city_list_iterate(pplayer->cities, other) {
    struct unit *cover;
    double swapped, lender, total;
    int rate, eta;

    if (other == pcity
        || tile_continent(city_tile(other))
           != tile_continent(city_tile(pcity))) {
      continue;
    }
    cover = deep_city_sparable_blade(ait, other, TRUE);
    if (cover == NULL || cover == blade) {
      continue;
    }
    /* Can it be here before the wave? The counter-claim's own
     * admission test, on the cover's own move rate -- a backfill that
     * arrives after the night it was meant to cover is not a cover.
     * (deep_relay_chain() bounds a hop in TILES instead; this is the
     * per-unit form, and it is the one a single move deserves.) */
    rate = MAX(unit_type_get(cover)->move_rate, 1);
    eta = (real_map_distance(city_tile(other), city_tile(pcity))
           * SINGLE_MOVE + rate - 1) / rate;
    if (eta > tune->survival_horizon) {
      continue;
    }
    swapped = deep_city_stack_survival_swap(nmap, pcity,
                                            tune->survival_horizon, blade,
                                            &cover, 1, NULL);
    lender = deep_city_stack_survival(nmap, other, tune->survival_horizon,
                                      NULL)
             - deep_city_stack_survival_without(nmap, other,
                                                tune->survival_horizon,
                                                cover, NULL);
    total = (base - swapped) + lender;
    if (total < out->cost) {
      out->cost = total;
      out->cover = cover;
      out->from = other;
      out->surv_after = swapped;
    }
  } city_list_iterate_end;

  deep_cover_priced++;
  if (out->cover != NULL) {
    deep_cover_found++;
    deep_cover_saved += out->naive_cost - out->cost;
    log_base(deep_metric_log_level(),
             "deep-cover turn=%d player=\"%s\" city=%d blade=\"%s\""
             " from=%d cover=\"%s\" naive=%.3f cost=%.3f surv=%.3f"
             " local=%.3f",
             game.info.turn, player_name(pplayer), pcity->id,
             unit_rule_name(blade), out->from->id,
             unit_rule_name(out->cover), out->naive_cost, out->cost,
             out->surv_after, naive);
  }

  return TRUE;
}

/**********************************************************************//**
  Order the covering move this price assumed. A one-edge brigade, run
  through deep_relay_execute() rather than through a second copy of
  the assign-guard/new-task pair, so the ordered move and the priced
  move are the same act by construction.

  Called only where the removal is COMMITTED in the same breath (the
  sortie). A consumer that may still refuse -- the operations draft --
  prices without ordering: opening a march for a hole that never opens
  is the trickle the muster discipline exists to prevent.
**************************************************************************/
void deep_cover_order(struct ai_type *ait, struct city *pcity,
                      const struct deep_cover_move *mv)
{
  struct city *path[2];

  if (mv == NULL || mv->cover == NULL || mv->from == NULL) {
    return;
  }
  path[0] = pcity;
  path[1] = mv->from;
  deep_relay_execute(ait, city_owner(pcity), path, 2);
}

/**********************************************************************//**
  The hole-covering census, on the birth guard's schedule.
**************************************************************************/
static void deep_cover_census(struct player *pplayer)
{
  if (deep_cover_priced > 0) {
    log_base(deep_metric_log_level(),
             "deep-cover-sum turn=%d player=\"%s\" priced=%d covered=%d"
             " saved=%.3f",
             game.info.turn, player_name(pplayer), deep_cover_priced,
             deep_cover_found, deep_cover_saved);
  }
  deep_cover_priced = 0;
  deep_cover_found = 0;
  deep_cover_saved = 0.0;
}

/* THE WRITE-OFF LEDGER. One entry per city-turn the counter-claim
 * machinery declares unholdable, resolved `horizon` turns later
 * against the only question that matters: did it actually fall?
 *
 * The write-off signal has been shipping since W14 and its PRECISION
 * has never been measured. Everything the brief wants to hang off it
 * -- evacuation, a fire sale -- destroys value irreversibly if the
 * signal is loose, so the census comes FIRST and the mechanism is
 * sized against what it says. Free: it allocates only when a write-off
 * happens and it runs with every lever off.
 *
 * Not saved. A reload loses the open entries, which costs the census
 * accuracy on the resolving turn and nothing else. */
struct deep_writeoff_memo {
  int plr_no;
  int city_id;
  int turn;                     /* when it was written off */
  int due;                      /* turn the verdict is taken */
  double p;
  double relief;                /* best survival the WHOLE pool reaches */
  int wave;
  int pool;                     /* reachable blades considered */
  int adjacent;                 /* at-war units on an adjacent tile */
  bool buyable;                 /* the treasury could buy this queue now */
};

/**********************************************************************//**
  At-war units standing on a tile adjacent to this city: the "final
  window" test, and the only one of the compound conditions that is
  about the enemy rather than about us.
**************************************************************************/
static int deep_enemy_adjacent(const struct city *pcity)
{
  const struct civ_map *nmap = &(wld.map);
  const struct player *pplayer = city_owner(pcity);
  int n = 0;

  adjc_iterate(nmap, city_tile(pcity), atile) {
    unit_list_iterate(atile->units, aunit) {
      if (pplayers_at_war(pplayer, unit_owner(aunit))
          && !unit_has_type_flag(aunit, UTYF_CIVILIAN)) {
        n++;
      }
    } unit_list_iterate_end;
  } adjc_iterate_end;

  return n;
}

/**********************************************************************//**
  Could the treasury rush this city's current production THIS turn?

  The predictive half of "the defence response has already been
  attempted". The write-off is taken in the first-activities pass and
  deep_spend_gold() runs at the END of the turn, so at write-off time
  the buy has not happened yet -- what can honestly be recorded is
  whether it COULD. A city whose queue is unbuyable, or whose price the
  treasury cannot meet, is a city the emergency buy will not save.
**************************************************************************/
static bool deep_city_buyable_now(const struct city *pcity)
{
  int cost = city_production_buy_gold_cost(pcity);

  return cost > 0 && cost <= city_owner(pcity)->economic.gold;
}

static struct deep_writeoff_memo *deep_writeoffs = NULL;
static int deep_writeoff_n = 0, deep_writeoff_cap = 0;

/**********************************************************************//**
  Record one write-off for later adjudication.
**************************************************************************/
static void deep_writeoff_record(const struct player *pplayer,
                                 struct city *pcity, double p,
                                 double relief, int wave, int pool,
                                 int horizon)
{
  struct deep_writeoff_memo *e;
  int i;

  for (i = 0; i < deep_writeoff_n; i++) {
    e = &deep_writeoffs[i];
    if (e->plr_no == player_number(pplayer) && e->city_id == pcity->id) {
      /* Already open. The FIRST verdict is the one being scored --
       * re-arming the clock every turn would make a long siege look
       * like one late call. */
      return;
    }
  }
  if (deep_writeoff_n >= deep_writeoff_cap) {
    deep_writeoff_cap = MAX(32, deep_writeoff_cap * 2);
    deep_writeoffs = fc_realloc(deep_writeoffs,
                                deep_writeoff_cap
                                * sizeof(*deep_writeoffs));
  }
  e = &deep_writeoffs[deep_writeoff_n++];
  e->plr_no = player_number(pplayer);
  e->city_id = pcity->id;
  e->turn = game.info.turn;
  e->due = game.info.turn + MAX(1, horizon);
  e->p = p;
  e->relief = relief;
  e->wave = wave;
  e->pool = pool;
  /* The compound condition is recorded AT THE CALL, not at the
   * verdict: what the consumer would have to act on is what was true
   * when the write-off was taken. */
  e->adjacent = deep_enemy_adjacent(pcity);
  e->buyable = deep_city_buyable_now(pcity);
}

/**********************************************************************//**
  THE EXHAUSTED SLICE (human call, 2026-08-07).

  The raw write-off signal fires BEFORE the defence machinery answers,
  and most flagged cities survive precisely BECAUSE the answer works --
  a blade arrives, a defender is bought, the wave turns aside. So the
  raw precision measured here is the precision of an ALARM and it is
  the wrong number to size an irreversible act against. The number that
  matters is the CONDITIONAL precision of the slice where every
  response has already been tried and the city is still doomed:

  - survival is still essentially zero;
  - the whole reachable relief pool, poach included, moves it by
    nothing (relief <= p) -- no reinforcement inside survival_horizon;
  - the treasury cannot rush this city's queue this turn;
  - and the enemy is ADJACENT, which is what makes it the final window
    rather than a forecast.

  Reported for every adjudication, so the conditional table can be
  computed from the same ledger that produces the raw one.
**************************************************************************/
static bool deep_writeoff_exhausted(const struct deep_writeoff_memo *e)
{
  return e->p < 0.01 && e->relief <= e->p + 0.001
         && e->adjacent > 0 && !e->buyable;
}

/**********************************************************************//**
  Adjudicate every write-off whose horizon has run out.

  TRUE  = the city is no longer ours (lost or destroyed) -- the signal
          was right.
  FALSE = we still hold it -- a FALSE write-off, and every turn of it
          is a turn a sale or an evacuation would have destroyed value
          in a city that went on to hold.

  Deliberately the plain ownership test and not "was it attacked":
  the consumer's decision is irreversible and the only thing that
  justifies it is the city actually changing hands.
**************************************************************************/
static void deep_writeoff_adjudicate(struct player *pplayer)
{
  int i = 0;

  while (i < deep_writeoff_n) {
    struct deep_writeoff_memo *e = &deep_writeoffs[i];

    if (e->plr_no != player_number(pplayer) || game.info.turn < e->due) {
      i++;
      continue;
    }
    log_base(deep_metric_log_level(),
             "deep-writeoff turn=%d player=\"%s\" city=%d called=%d"
             " p=%.3f relief=%.3f wave=%d pool=%d adj=%d buy=%d"
             " exhausted=%d lost=%d",
             game.info.turn, player_name(pplayer), e->city_id, e->turn,
             e->p, e->relief, e->wave, e->pool, e->adjacent,
             e->buyable ? 1 : 0,
             deep_writeoff_exhausted(e) ? 1 : 0,
             player_city_by_number(pplayer, e->city_id) == NULL ? 1 : 0);
    deep_writeoffs[i] = deep_writeoffs[--deep_writeoff_n];
  }
}

/* Rung 2a's census. */
static int deep_evac_cands = 0, deep_evac_moved = 0;

/**********************************************************************//**
  RUNG 2a: GET THE CIVILIANS OUT (human refinement, 2026-08-07).

  Evacuation is split from the defensive write-off because the two have
  completely different bars, and the census is why. Conditioning the
  write-off on "no reinforcement can reach it AND the enemy is
  adjacent" lifts its precision from 0.027 to 0.202 (contested, 40
  games) -- a 7x lift, and still TWO IN THREE of those cities hold.
  Any irreversible act at that precision loots two survivors per
  casualty, which is why the denial sale (2b) has to beat
  stand-and-fight in a priced comparison instead of firing on a gate.

  A civilian walking out is not that act. It contributes exactly zero
  to the defence -- deep_collect_defenders() skips UTYF_CIVILIAN, so
  the survival probability this pass just computed is unchanged by its
  leaving -- and it dies with the city if it stays. The cost of being
  wrong is a few unit-turns of walking, so this half keeps the LOOSE
  compound gate: written off, survival essentially zero, and no
  reinforcement inside the horizon that moves it.

  The move is the flee machinery's own recipe, unchanged: cancel the
  activity, dai_unit_goto(), hold. The refuge is the nearest own city
  on this continent that is not itself being written off this turn --
  walking a worker from one doomed city into another is not an
  evacuation.

  evacuate_gate 0 = off, and the pass returns before reading a unit.
**************************************************************************/
static void deep_evacuate_civilians(struct ai_type *ait,
                                    struct player *pplayer,
                                    struct city *pcity, double p,
                                    double relieved, int wave)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct tile *ctile = city_tile(pcity);
  int moved = 0;

  if (tune->evacuate_gate <= 0
      || p >= 0.01 || relieved > p + 0.001) {
    return;                     /* Not the loose compound gate. */
  }

  unit_list_iterate_safe(ctile->units, u) {
    struct city *refuge = NULL;
    int bestd = -1;

    if (unit_owner(u) != pplayer
        || !unit_has_type_flag(u, UTYF_CIVILIAN)
        || unit_transported(u)
        || deep_phase_held(pplayer, u->id)) {
      continue;
    }
    deep_evac_cands++;

    city_list_iterate(pplayer->cities, ocity) {
      int d;

      if (ocity == pcity
          || tile_continent(city_tile(ocity)) != tile_continent(ctile)) {
        continue;
      }
      /* Not into another city that is falling tonight. */
      if (deep_city_survival_verdict(nmap, ocity, tune->survival_horizon,
                                     NULL) < tune->survival_target) {
        continue;
      }
      d = real_map_distance(ctile, city_tile(ocity));
      if (bestd < 0 || d < bestd) {
        bestd = d;
        refuge = ocity;
      }
    } city_list_iterate_end;

    if (refuge == NULL) {
      continue;                 /* Nowhere better to be. */
    }
    unit_activity_handling(u, ACTIVITY_IDLE, ACTION_NONE);
    if (dai_unit_goto(ait, u, city_tile(refuge))) {
      deep_hold_unit(ait, u, "evac");
      moved++;
      deep_evac_moved++;
      log_base(deep_metric_log_level(),
               "deep-evac turn=%d player=\"%s\" city=%d unit=\"%s\""
               " to=%d dist=%d p=%.3f wave=%d",
               game.info.turn, player_name(pplayer), pcity->id,
               unit_rule_name(u), refuge->id, bestd, p, wave);
    }
  } unit_list_iterate_safe_end;

  if (moved > 0) {
    log_base(deep_metric_log_level(),
             "deep-evac-city turn=%d player=\"%s\" city=%d moved=%d",
             game.info.turn, player_name(pplayer), pcity->id, moved);
  }
}

static void deep_defend_counter_claims(struct ai_type *ait,
                                       struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  /* Before this turn's verdicts, so an entry is never adjudicated on
   * the turn it is written. */
  deep_writeoff_adjudicate(pplayer);
  if (deep_evac_cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-evac-sum turn=%d player=\"%s\" cands=%d moved=%d",
             game.info.turn, player_name(pplayer), deep_evac_cands,
             deep_evac_moved);
  }
  deep_evac_cands = 0;
  deep_evac_moved = 0;

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
          || !deep_utype_is_land(unit_type_get(u))
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
      deep_writeoff_record(pplayer, pcity, p, relieved, wave, n_cand,
                           tune->survival_horizon);
      deep_evacuate_civilians(ait, pplayer, pcity, p, relieved, wave);
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
/**********************************************************************//**
  Pillage as yield denial (S-GROUND item 2).

  The spec for this is a comment in the shipped advisor:
  `autoworkers.c:128`, *"We could have ACTRES_PILLAGE here, but
  currently we don't"*. The only pillage anywhere in the tree is
  `daiunit.c:930-938` -- barbarian-only, unconditional, no target
  evaluation -- and `ai/deep` had zero pillage references. Meanwhile
  664 roads and 371 irrigations stood undefended at T162 in a measured
  classic game, and rampagers go home when there is nothing left to
  kill.

  Deep values a pillage at what it DENIES: the output the tile stops
  producing for its owner, summed over food, shield and trade, read
  from the ruleset by rebuilding the tile without the extra. Omniscient
  today (the tile output is read directly); a known-map version feeds
  the same arithmetic from the city knowledge layer and is the seam,
  not a different mechanism.

  Scope, stated and narrow. Only a unit ALREADY STANDING on an
  eligible tile is acted on, which needs no movement logic and cannot
  send a blade anywhere. The larger population -- idle blades within
  raid_radius of an eligible tile, which is what a pillage RAID would
  act on -- is counted and not built (W36 discipline: size the
  machinery against its population first).

  Tier discipline: war only, and the pass runs after every claiming
  pass, skipping anything the melt, the sorties, the counter-claims or
  the operation layer took. A pillage can never be chosen instead of a
  live kill or a live defence.
**************************************************************************/
static double deep_pillage_denial(const struct tile *ptile,
                                  struct extra_type *pextra)
{
  struct tile *vtile = tile_virtual_new(ptile);
  double lost = 0.0;
  Output_type_id o;

  tile_remove_extra(vtile, pextra);
  for (o = 0; o < O_LAST; o++) {
    if (o == O_FOOD || o == O_SHIELD || o == O_TRADE) {
      lost += city_tile_output(NULL, ptile, FALSE, o)
              - city_tile_output(NULL, vtile, FALSE, o);
    }
  }
  tile_virtual_destroy(vtile);

  return lost;
}

static void deep_pillage_sorties(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int started = 0, standing = 0, nearby = 0;

  if (tune->pillage_denial_gain <= 0.0) {
    return;
  }

  unit_list_iterate(pplayer->units, punit) {
    struct tile *ptile = unit_tile(punit);
    struct extra_type *best = NULL;
    double best_want = 0.0;
    struct player *victim;

    if (is_special_unit(punit)
        || !is_attack_unit(punit)
        || unit_has_orders(punit)
        || punit->activity == ACTIVITY_PILLAGE
        || deep_op_claiming(pplayer, punit->id) >= 0
        || deep_phase_held(pplayer, punit->id)) {
      continue;
    }

    victim = tile_owner(ptile);
    if (victim == NULL || victim == pplayer
        || !pplayers_at_war(pplayer, victim)) {
      /* The census half: an eligible tile we are merely NEAR is the
       * raid population, counted so the raid can be sized before it
       * is built. */
      if (tune->raid_radius > 0) {
        square_iterate(nmap, ptile, tune->raid_radius, atile) {
          struct player *aowner = tile_owner(atile);

          if (aowner != NULL && aowner != pplayer
              && pplayers_at_war(pplayer, aowner)) {
            extra_type_by_rmcause_iterate(ERM_PILLAGE, px) {
              if (tile_has_extra(atile, px)) {
                nearby++;
                break;
              }
            } extra_type_by_rmcause_iterate_end;
            break;
          }
        } square_iterate_end;
      }
      continue;
    }

    standing++;
    extra_type_by_rmcause_iterate(ERM_PILLAGE, pextra) {
      enum gen_action act = activity_default_action(ACTIVITY_PILLAGE);
      double want;

      if (!tile_has_extra(ptile, pextra)
          || !can_unit_do_activity_targeted(nmap, punit, ACTIVITY_PILLAGE,
                                            act, pextra)) {
        continue;
      }
      want = tune->pillage_denial_gain
             * deep_pillage_denial(ptile, pextra);
      if (want > best_want) {
        best_want = want;
        best = pextra;
      }
    } extra_type_by_rmcause_iterate_end;

    if (best != NULL && best_want >= 1.0) {
      if (unit_activity_handling_targeted(punit, ACTIVITY_PILLAGE, &best,
                                          activity_default_action(ACTIVITY_PILLAGE))) {
        deep_hold_unit(ait, punit, "pillage");
        started++;
      }
    }
  } unit_list_iterate_end;

  if ((started > 0 || nearby > 0)
      && log_do_output_for_level(deep_metric_log_level())) {
    log_base(deep_metric_log_level(),
             "deep-pillage turn=%d player=\"%s\" standing=%d started=%d"
             " nearby=%d",
             game.info.turn, player_name(pplayer), standing, started,
             nearby);
  }
}

/**********************************************************************//**
  Fortify on ground worth holding (S-GROUND item 4).

  Stock reaches ACTIVITY_FORTIFYING only as the terminal fallback of
  `dai_manage_military()` (`daiunit.c:2694-2713`), keyed on
  AIUNIT_DEFEND_HOME -- never for terrain, never for a chokepoint. A
  unit standing in a fortress or in a gate through the landmass is
  therefore as likely to wander off it as to dig in, which makes the
  fortresses the previous commits taught deep to BUILD worth less than
  they cost.

  The consumer half, and deliberately small: a military unit already
  standing on ground this session made wantable -- a defensive base,
  or a chokepoint -- fortifies and takes a phase hold so the manage
  loop leaves it there.

  TIER DISCIPLINE, and it is the whole safety argument: this runs
  AFTER every pass that can claim a blade for something live (the melt,
  the sorties, the counter-claims and the operation layer), and it
  skips any unit those passes claimed or held. So it can only ever
  move a unit that was about to wander, never one with a kill or a
  defence to make. It also skips city tiles, which are the garrison
  ladder's business, and units with a live goto.
**************************************************************************/
static void deep_fortify_ground(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int held = 0, seen = 0;

  if (tune->fortify_ground <= 0) {
    return;
  }

  unit_list_iterate(pplayer->units, punit) {
    struct tile *ptile = unit_tile(punit);
    bool worth;

    if (is_special_unit(punit)
        || !is_attack_unit(punit)
        || tile_city(ptile) != NULL
        || punit->activity == ACTIVITY_FORTIFIED
        || unit_has_orders(punit)
        || deep_op_claiming(pplayer, punit->id) >= 0
        || deep_phase_held(pplayer, punit->id)) {
      continue;
    }

    /* Ground this session taught deep to value: a base that actually
     * defends the unit standing on it, or a gate through the land. */
    worth = FALSE;
    extra_type_by_cause_iterate(EC_BASE, pextra) {
      if (tile_has_extra(ptile, pextra)
          && is_native_extra_to_uclass(pextra, unit_class_get(punit))) {
        worth = TRUE;
        break;
      }
    } extra_type_by_cause_iterate_end;
    if (!worth) {
      worth = deep_site_is_choke(nmap, ptile);
    }
    if (!worth) {
      continue;
    }

    seen++;
    if (punit->activity == ACTIVITY_IDLE
        || punit->activity == ACTIVITY_SENTRY) {
      unit_activity_handling(punit, ACTIVITY_FORTIFYING,
                             activity_default_action(ACTIVITY_FORTIFYING));
    }
    deep_hold_unit(ait, punit, "ground");
    held++;
  } unit_list_iterate_end;

  if (held > 0 && log_do_output_for_level(deep_metric_log_level())) {
    log_base(deep_metric_log_level(),
             "deep-ground turn=%d player=\"%s\" eligible=%d held=%d",
             game.info.turn, player_name(pplayer), seen, held);
  }
}

void deep_do_first_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  dai_assess_danger_player(ait, &(wld.map), pplayer);

  deep_log_defence(ait, pplayer);
  deep_capture_census(ait, pplayer);
  deep_barb_census(pplayer);
  deep_credit_incoming_defence(ait, pplayer);
  deep_melt_surplus_defenders(ait, pplayer);
  deep_manage_convoys(ait, pplayer);
  deep_hold_tired_attackers(ait, pplayer);
  deep_flee_workers(ait, pplayer);
  deep_kill_sorties(ait, pplayer);
  deep_cover_sorties(ait, pplayer);
  deep_defend_counter_claims(ait, pplayer);
  deep_run_operations(ait, pplayer);
  /* LAST of the claiming passes, so it can only catch a blade that
   * was about to wander. */
  deep_pillage_sorties(ait, pplayer);
  deep_fortify_ground(ait, pplayer);
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
  expansion_gate_enabled = 0 removes the gate altogether -- neither
  deep's survival test nor stock's war-footing test is consulted and no
  city ever stops expanding for war. That third arm exists because the
  probe that priced deep's gate against stock's (+149/seed for deep;
  findings/era-probe-evidence-2026-08-12.md S4) could not ask what a
  world with no gate scores, there being no code path for it.
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

/* The per-unit predicate behind both counters below, factored out so
 * "how many hulls" and "how many berths" cannot form two opinions
 * about which hull is idle on which water. */
static bool deep_ferry_idle_here(struct ai_type *ait, struct player *pplayer,
                                 struct unit *punit, Continent_id ocean)
{
  const struct unit_ai *ud = def_ai_unit_data(punit, ait);

  if (!dai_is_ferry(punit, ait)
      || get_transporter_occupancy(punit) > 0
      || ud->passenger >= 0
      || ud->task == AIUNIT_EXPLORE
      || punit->ssa_controller == SSA_AUTOEXPLORE
      || deep_op_claiming(pplayer, punit->id) >= 0) {
    return FALSE;
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
      return FALSE;
    }
  }

  return TRUE;
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
    if (deep_ferry_idle_here(ait, pplayer, punit, ocean)) {
      idle++;
    }
  } unit_list_iterate_end;

  return idle;
}

/**********************************************************************//**
  The founders -- and only the founders -- waiting to cross this water.

  deep_ferry_demand_on() counts every unit waiting for a crossing, which
  is the right denominator for a fleet but the wrong numerator for an
  expansion decision: after t20 the home continent produces no legal
  site at all on a settled venue (EXPANSION slice 1: 0.0 scored land
  candidates per search from t20 on both seeds, 89% of refusals the
  citymindist ring), so every founder want deep still has is an
  OVERSEAS want and the lift is the only channel it has. What the
  shipyard needs to know is how much of that queue is settlement.

  Same waiting predicate as deep_ferry_demand_on(), plus two clauses:

  * the unit can found a city -- ACTION_FOUND_CITY, the ruleset's own
    answer, not a unit-type guess;
  * no operation has claimed it. A claimed passenger is being served,
    and this is the definition deep_wait_spells_update() already uses
    to END a spell, so the gap and the wait estimate count the same
    population. deep_ferry_demand_on() deliberately does NOT exclude
    claimed units, because a hull's idleness is measured against
    everyone aboard the queue; the divergence is intentional.
**************************************************************************/
int deep_founder_demand_on(struct ai_type *ait, struct player *pplayer,
                           Continent_id ocean)
{
  const struct civ_map *nmap = &(wld.map);
  int want = 0;

  unit_list_iterate(pplayer->units, punit) {
    bool touches = FALSE;

    if (unit_transported(punit)
        || def_ai_unit_data(punit, ait)->ferryboat >= 0
        || punit->goto_tile == NULL
        || tile_continent(unit_tile(punit))
           == tile_continent(punit->goto_tile)
        || !unit_can_do_action(punit, ACTION_FOUND_CITY)
        || deep_op_claiming(pplayer, punit->id) >= 0) {
      continue;
    }
    adjc_iterate(nmap, unit_tile(punit), atile) {
      if (tile_continent(atile) == ocean) {
        touches = TRUE;
        break;
      }
    } adjc_iterate_end;
    if (touches) {
      want++;
    }
  } unit_list_iterate_end;

  return want;
}

/**********************************************************************//**
  Lift supply on this water, in BERTHS rather than hulls: the capacity
  standing idle on it now, and (through `building`, when non-NULL) the
  capacity our own cities on the same water are already producing.

  Berths because the demand is passengers. A hull that carries four is
  not one unit of supply against four waiting founders, and counting
  hulls is what made every previous damping unable to name a target
  ("fewer than now" is not a number).

  The building term is what keeps a gap from being sold twice: every
  coastal city on a water reads the same queue in the same turn, and
  without it each of them would commission its own hull for the same
  passengers. It counts orders, not deliveries -- a city that switches
  production drops out of the count the same turn.
**************************************************************************/
int deep_lift_berths_on(struct ai_type *ait, struct player *pplayer,
                        Continent_id ocean, int *building)
{
  const struct civ_map *nmap = &(wld.map);
  int berths = 0;

  unit_list_iterate(pplayer->units, punit) {
    if (deep_ferry_idle_here(ait, pplayer, punit, ocean)) {
      berths += get_transporter_capacity(punit);
    }
  } unit_list_iterate_end;

  if (building != NULL) {
    *building = 0;
    city_list_iterate(pplayer->cities, pcity) {
      const struct unit_type *ptype;
      bool touches = FALSE;

      if (VUT_UTYPE != pcity->production.kind) {
        continue;
      }
      ptype = pcity->production.value.utype;
      if (ptype->transport_capacity <= 0 || !deep_utype_is_sea(ptype)) {
        continue;
      }
      adjc_iterate(nmap, city_tile(pcity), atile) {
        if (tile_continent(atile) == ocean) {
          touches = TRUE;
          break;
        }
      } adjc_iterate_end;
      if (touches) {
        *building += ptype->transport_capacity;
      }
    } city_list_iterate_end;
  }

  return berths;
}

/* Observation only (ERA): one line per gate evaluation, in deep's
 * metric style, joinable to deep-def / deep-surv on (turn, city).
 *
 * `stock_blocked` is the counterfactual this counter exists for --
 * what the gate deep REPLACED would have said on the same city-turn.
 * That verdict is dai_on_war_footing() alone, which the live path has
 * already computed and which is a pure read of the diplomacy
 * countdowns, so the counterfactual costs nothing and needs neither
 * ai/baseline nor a second run. It is emitted beside `footing` (the
 * same bool) deliberately: the analysis reads one field per question.
 *
 * `mil` is the gate's second conjunct at the call site, so the
 * EFFECTIVE firing rate (blocked && mil > 0) is derivable from the
 * line rather than inferred. wave=-1 / surv=-1 mean the survival
 * model was never asked on this path; wave=-2 means it never will be,
 * because expansion_gate_enabled = 0 has removed the gate. The off
 * arm still emits a line so that `stock_blocked` and the two wants
 * keep pricing what the gate WOULD have cost on the same city-turns.
 *
 * `mil` is choice.want, an adv_want, i.e. a FLOAT
 * (common/fc_types.h). It takes a float conversion, and it was given
 * %d: the promoted double travels in a different register class from
 * the integers, so that conversion consumed the NEXT integer argument
 * and every conversion after it shifted by one. The 2026-08-12 era
 * probe caught it -- `mil` was carrying city_size_get() and `size`
 * was carrying uninitialised stack
 * (findings/era-probe-evidence-2026-08-12.md, section 5). Every field
 * ahead of `mil` was sound; the gate's second conjunct was
 * unobservable for the whole probe.
 *
 * Every value here is already computed by the decision it watches;
 * nothing is recomputed (deep_bid_log's rule). */
static void deep_expgate_log(struct player *pplayer,
                             const struct city *pcity,
                             const struct ai_city *city_data,
                             bool footing, int wave, double surv,
                             double thresh, bool blocked)
{
  log_base(deep_metric_log_level(),
           "deep-expgate turn=%d player=\"%s\" city=%d footing=%d"
           " wave=%d surv=%.3f thresh=%.2f blocked=%d stock_blocked=%d"
           " founder=%d worker=%d mil=%.1f size=%d",
           game.info.turn, player_name(pplayer), pcity->id,
           footing ? 1 : 0, wave, surv, thresh, blocked ? 1 : 0,
           footing ? 1 : 0, city_data->founder_want,
           city_data->worker_want, city_data->choice.want,
           city_size_get(pcity));
}

static bool deep_expansion_blocked(struct ai_type *ait,
                                   struct player *pplayer,
                                   struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct ai_city *city_data = def_ai_city_data(pcity, ait);
  /* Hoisted, not added: every path below asked this exactly once
   * already, and it is a pure scan of the countdowns. */
  bool footing = dai_on_war_footing(ait, pplayer);
  int wave = 0;
  double surv;
  bool blocked;

  if (tune->expansion_gate_enabled == 0) {
    deep_expgate_log(pplayer, pcity, city_data, footing, -2, -1.0,
                     tune->expansion_war_survival, FALSE);
    return FALSE;
  }
  if (tune->expansion_war_survival <= 0.0) {
    deep_expgate_log(pplayer, pcity, city_data, footing, -1, -1.0,
                     tune->expansion_war_survival, footing);
    return footing;
  }
  if (!footing) {
    deep_expgate_log(pplayer, pcity, city_data, footing, -1, -1.0,
                     tune->expansion_war_survival, FALSE);
    return FALSE;
  }
  /* On war footing, but only cities that would actually FALL stop
   * planning for the future. A safe city in a warring empire still
   * settles and still improves its land. */
  surv = deep_city_stack_survival(&(wld.map), pcity,
                                  tune->survival_horizon, &wave);
  blocked = (wave > 0 && surv < tune->expansion_war_survival);
  deep_expgate_log(pplayer, pcity, city_data, footing, wave, surv,
                   tune->expansion_war_survival, blocked);

  return blocked;
}

/* Birth-guard census, per player per turn: how often the inherited
 * rule would have fired, how often it was PRICED instead, what the
 * price came to, and how often the emergency escalation was left
 * alone because the threat was real. */
static int deep_birth_cands = 0, deep_birth_priced = 0;
static int deep_birth_urgent = 0;
static double deep_birth_paid = 0.0, deep_birth_odds = 0.0;

/* The deterrence dividend's counters, reset on the same schedule. */
static int deep_deter_priced = 0;
static double deep_deter_paid = 0.0;

/* And the attacker's garrison credit (NETTHREAT item 3): attacker
 * wants that actually received it, what it paid, and -- the counter
 * that says whether the credit ever CHANGED anything -- the wants it
 * carried past the defender choice they were competing with. */
static int deep_agc_priced = 0;
static double deep_agc_paid = 0.0;

/* The lifetime-upkeep charge's counters, likewise. */
static int deep_upcharge_n = 0;
static double deep_upcharge_paid = 0.0;

/**********************************************************************//**
  What a first defender in an EMPTY city is actually worth.

  THE RULE IT REPLACES (inherited from military_advisor_choose_build()):
  with `our_def == 0` and any danger at all, stock pins danger to a flat
  100 when `urgency == 0` and hands the choice DWT_EMERGENCY. Three
  faults, and the human named the first on 2026-08-06 ("that tier 1
  thing is dumb"):

  - THREAT-BLIND. The 100 is a constant. It does not read who is
    coming, from how far, or whether anyone can arrive at all. And
    deep's own fear lattice guarantees the trigger: the density prior,
    the boat shadow and the barbarian ramp all put standing danger on a
    city that has met nobody, so `danger > 0` is true of every newborn
    city and the rule fires cohort-wide. Derby 3 measured what that
    looks like -- one Warriors per city at turn 2 across the whole
    cohort, 39 units by T16 against stock's 0.
  - STANCE-PROOF. DWT_EMERGENCY outranks every NORMAL want by tier, so
    W34's gates cannot touch it; an early-peace personality and an
    early-war one build the identical turn-2 Warriors.
  - UNPRICED. It is charged against the settler tempo it displaces and
    never compared to it.

  THE REPLACEMENT: want = P(walk-in) x city worth x birth_guard_gain,
  at DWT_NORMAL, through the stance fear gate.

  P(walk-in) is the union of two independent events, both read from
  machinery that already exists and neither invented here:

  - THE REACHABILITY LAW. `1 - deep_city_stack_survival()`. For a city
    with an empty garrison that function is exactly 0.0 against any
    wave and exactly 1.0 against none (deepcombat.c), and its wave is
    built from at-war, CONTACT-MADE units that can arrive inside
    survival_horizon. So a city nobody has met and nobody can reach
    prices at zero and the settler wins the auction, while a warband
    two tiles out puts the term at one -- and by then `urgency > 0`
    anyway and the emergency tier fires on the branch below, which is
    left exactly as it was.
  - THE BARB GATE. `deep_barb_walkin_odds()`: barbarians never make
    contact, so they are absent from the wave by construction, and an
    uprising is the one walk-in that arrives from nowhere. The odds are
    the server's own three probability factors.

  City worth is stock's `ai_city.worth` -- the same number
  deep_conquest_worth() prices a CAPTURE at, so what we pay to keep a
  city and what an attacker pays to take it are quoted in one currency
  for the first time.

  birth_guard_gain = 0 (the compiled default) restores the inherited
  rule byte for byte, including its flat 100 and its tier.
**************************************************************************/
static bool deep_birth_guard_want(struct ai_type *ait,
                                  const struct civ_map *nmap,
                                  struct city *pcity, unsigned int urgency,
                                  double *want_out)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  double surv, p_mil, p_barb, p, worth;
  int wave = 0;

  deep_birth_cands++;
  if (urgency > 0) {
    /* A visible, urgent attacker. The emergency tier is the right
     * answer here and this pass does not touch it. */
    deep_birth_urgent++;
    return FALSE;
  }
  if (tune->birth_guard_gain <= 0.0) {
    return FALSE;
  }

  surv = deep_city_survival_verdict(nmap, pcity, tune->survival_horizon,
                                    &wave);
  p_mil = wave > 0 ? 1.0 - surv : 0.0;
  p_barb = deep_barb_walkin_odds(pcity);
  p = 1.0 - (1.0 - p_mil) * (1.0 - p_barb);
  worth = def_ai_city_data(pcity, ait)->worth;

  *want_out = p * worth * tune->birth_guard_gain
              * deep_stance_fear_mult(pplayer);

  deep_birth_priced++;
  deep_birth_odds += p;
  deep_birth_paid += *want_out;

  log_base(deep_metric_log_level(),
           "deep-birth turn=%d player=\"%s\" city=%d wave=%d surv=%.3f"
           " pmil=%.3f pbarb=%.3f p=%.3f worth=%.1f want=%.1f",
           game.info.turn, player_name(pplayer), pcity->id, wave, surv,
           p_mil, p_barb, p, worth, *want_out);

  return TRUE;
}

/**********************************************************************//**
  The birth guard's execution counter, one line per player per turn.
  Priced against escalated is the A/B lens the brief asks for: a cell
  where `urgent` is most of `cands` is a cell where the inherited rule
  was mostly right.
**************************************************************************/
static void deep_birth_census(struct player *pplayer)
{
  if (deep_birth_cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-birthguard turn=%d player=\"%s\" cands=%d priced=%d"
             " urgent=%d meanp=%.3f paid=%.1f",
             game.info.turn, player_name(pplayer), deep_birth_cands,
             deep_birth_priced, deep_birth_urgent,
             deep_birth_priced > 0
             ? deep_birth_odds / deep_birth_priced : 0.0,
             deep_birth_paid);
  }
  deep_birth_cands = 0;
  deep_birth_priced = 0;
  deep_birth_urgent = 0;
  deep_birth_odds = 0.0;
  deep_birth_paid = 0.0;

  /* The dividend's own line, on the same schedule. Zero lines a game
   * with deterrence_gain at its default is half the off-switch
   * proof; the other half is that the two DP runs never happen. */
  if (deep_deter_priced > 0) {
    log_base(deep_metric_log_level(),
             "deep-deter-sum turn=%d player=\"%s\" priced=%d paid=%.1f",
             game.info.turn, player_name(pplayer), deep_deter_priced,
             deep_deter_paid);
  }
  deep_deter_priced = 0;
  deep_deter_paid = 0.0;

  /* The attacker garrison credit's line, same schedule and the same
   * contract: zero lines with attacker_garrison_gain at its default,
   * because the credit's own reads never happen. */
  if (deep_agc_priced > 0) {
    log_base(deep_metric_log_level(),
             "deep-agc-sum turn=%d player=\"%s\" priced=%d paid=%.1f"
             " home_share=%.3f",
             game.info.turn, player_name(pplayer), deep_agc_priced,
             deep_agc_paid, deep_attacker_home_share(pplayer));
  }
  deep_agc_priced = 0;
  deep_agc_paid = 0.0;

  /* The lifetime-upkeep charge's line, same schedule. `n` is the
   * population the mechanism acts on -- the number the audit's claim
   * that it "moves EVERY unit want" is checked against. */
  if (deep_upcharge_n > 0) {
    log_base(deep_metric_log_level(),
             "deep-upcharge-sum turn=%d player=\"%s\" n=%d paid=%.1f",
             game.info.turn, player_name(pplayer), deep_upcharge_n,
             deep_upcharge_paid);
  }
  deep_upcharge_n = 0;
  deep_upcharge_paid = 0.0;

  /* And the hole-covering price's, same schedule and same contract. */
  deep_cover_census(pplayer);
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
/**********************************************************************//**
  THE DETERRENCE DIVIDEND (M4b item 2; human 2026-08-06: "we aren't
  valuing defensive units highly enough... a musket behind walls is
  pretty dang strong").

  Every price this module puts on a defender is a price on WAVE
  SURVIVAL -- the survival deficit fear, the garrison floor, the
  counter-claim -- so a blade that is going to lose is worth nothing,
  and that is simply false. It is worth the attacker shields it
  destroys on the way down. CONSEQ's fight-vs-deny design named the
  same quantity from the other side ("what resisting COSTS the taker",
  findings/conseq-wrace §2b) and this is that term, consumed rather
  than reimplemented.

  The honest price of ONE MORE defender is marginal, so it is priced
  marginally: run the chained-duel model against the wave twice (an
  omniscient read with no visibility test -- RC6, findings/
  omniscience-census-2026-08-11.md S5), once with a virtual unit of
  the candidate type standing in the city and once without, and take
  the difference in expected attacker shields destroyed. The tile-
  aware get_total_defense_power() seam (deepcombat.c:174) is inside
  that read, which is what makes "behind walls" arithmetic rather
  than rhetoric: the same musket prices higher in a walled city
  because its duels really are better.

  Times the attack pressure deep_garrison_menace() reads from
  aplayer->score.units -- an omniscient read of the rival's own score
  field with no visibility test of any kind (RC6; findings/
  omniscience-census-2026-08-11.md S5), not anything observed -- the
  same 0..1 read the garrison floor is already sized by, so the term
  that insures against a threat and the term that prices killing it
  agree about how much threat there is.

  NOT double-counting defence_worth_mult: that multiplier prices the
  CITY at the counter-claim's stakes read. This prices the ATTRITION,
  a disjoint quantity, and the two are added by different seams.

  Returns 0 and costs nothing when the city faces no wave, which is
  every city in peacetime -- the two DP runs never happen.
**************************************************************************/
static double deep_marginal_attrition(const struct civ_map *nmap,
                                      struct city *pcity,
                                      const struct unit_type *ut,
                                      struct deep_defence_read *now_out)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_defence_read now, plus;
  struct unit *v;
  double marginal;

  /* The verdict this phase already took: no wave, no attrition to
   * price, and no DP runs at all. */
  {
    int wave = 0;

    (void) deep_city_survival_verdict(nmap, pcity, tune->survival_horizon,
                                      &wave);
    if (wave <= 0) {
      return 0.0;
    }
  }

  deep_city_defence_read(nmap, pcity, tune->survival_horizon, NULL, 0,
                         &now);
  v = unit_virtual_create(city_owner(pcity), pcity,
                          (struct unit_type *) ut, 0);
  unit_tile_set(v, city_tile(pcity));
  deep_city_defence_read(nmap, pcity, tune->survival_horizon, &v, 1,
                         &plus);
  unit_virtual_destroy(v);

  marginal = plus.att_loss - now.att_loss;
  if (marginal <= 0.0) {
    /* The stack is already deep enough that the wave dies before this
     * body is reached, or the caps truncated it. Nothing owed. */
    return 0.0;
  }
  if (now_out != NULL) {
    *now_out = now;
  }

  return marginal;
}

static adv_want deep_deterrence_dividend(const struct civ_map *nmap,
                                         struct city *pcity,
                                         const struct unit_type *ut,
                                         double menace)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_defence_read now = { 1.0, 0, 0, 0.0, 0.0 };
  double marginal;
  adv_want div;

  if (tune->deterrence_gain <= 0.0 || ut == NULL || menace <= 0.0) {
    return 0;
  }
  marginal = deep_marginal_attrition(nmap, pcity, ut, &now);
  if (marginal <= 0.0) {
    return 0;
  }
  div = (adv_want) (marginal * menace * tune->deterrence_gain);
  if (div <= 0) {
    return 0;
  }

  deep_deter_priced++;
  deep_deter_paid += div;
  log_base(deep_metric_log_level(),
           "deep-deter turn=%d player=\"%s\" city=%d type=\"%s\""
           " wave=%d marg=%.1f menace=%.3f surv=%.3f div=%.1f",
           game.info.turn, player_name(city_owner(pcity)), pcity->id,
           utype_rule_name(ut), now.wave, marginal, menace, now.surv,
           (double) div);

  return div;
}

/**********************************************************************//**
  THE ATTACKER'S GARRISON CREDIT (NETTHREAT item 3, promoted to lead by
  the human 2026-08-12: "an attack unit's defensive value is the
  under-credited half, so credit it first").

  A home-based attacker joins the stack the moment it is standing at
  home -- deep_city_survival_core() counts it, the sortie pass spends
  it, the garrison floor counts it -- but at the BUILD seam an attacker
  and a defender were compared as orthogonal goods: the defender want
  carries survival plus the deterrence dividend, and the attacker want
  carried take-odds alone. The defensive worth of an attacker was
  exactly zero in the one comparison where it decides what gets built.

  Same term as the dividend, from the same reads
  (deep_marginal_attrition(), literally the shared helper), times the
  same menace -- so the two sides of the auction are finally quoted in
  one currency -- and then times the OBSERVED share of unit-turns this
  player's attack-rated units spend in own cities.

  ------------------------------------------------------------------
  THE BOOKKEEPING RULE, stated where the credit lands (the guard the
  brief makes load-bearing). Three books touch this quantity and each
  enemy body, and each of OUR bodies, is charged to exactly one:

  1. vs deterrence_gain. The dividend is paid to the DEFENDER choice,
     for the defender body. This is paid to the ATTACKER choice, for
     the attacker body. They are competing bids for one build slot;
     at most one of the two bodies is ever built, so the attrition is
     credited once per body that comes to exist. Never both, never
     twice.

  2. vs the sortie prize (sortie_prize_gain). That gain never enters a
     build want at all -- it is an execution-time selection score that
     ranks which duel an ALREADY EXISTING blade should fight. Nothing
     sums the two. And the sortie half of "garrison/sortie
     contribution" is deliberately NOT priced here: crediting the
     build with a kill the sortie pass will also value would be the
     one double count available, so this credits the GARRISON half
     only and under-credits on purpose.

  3. vs this want's own offensive half. gross/marginal above prices
     this unit as MARCHING to a target. This prices it as STANDING AT
     HOME. They are mutually exclusive uses of one body, which is
     exactly why the credit is weighted by home_share < 1 and the
     offensive half is not scaled up to compensate: the sum of the two
     books is at most one unit's presence, and in practice less.
  ------------------------------------------------------------------

  Applied AFTER gate 5 and the never-launches damper, on purpose. Both
  of those price OFFENSIVE appetite -- how much this player wants to
  buy an assault, and how badly its assaults stall in muster -- and an
  attacker's garrison value is precisely what an op that never launches
  still leaves behind. Scaling this by the damper would say a unit that
  stays home is worth less for staying home.

  0 = off, and off costs nothing: the gain is tested before any read.
**************************************************************************/
static adv_want deep_attacker_garrison_credit(const struct civ_map *nmap,
                                              struct city *pcity,
                                              const struct unit_type *ut,
                                              double menace)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_defence_read now = { 1.0, 0, 0, 0.0, 0.0 };
  struct player *pplayer = city_owner(pcity);
  double marginal, share;
  adv_want credit;

  if (tune->attacker_garrison_gain <= 0.0 || ut == NULL || menace <= 0.0) {
    return 0;
  }
  share = deep_attacker_home_share(pplayer);
  if (share <= 0.0) {
    /* No cadence observed yet -- no evidence, no credit. */
    return 0;
  }
  marginal = deep_marginal_attrition(nmap, pcity, ut, &now);
  if (marginal <= 0.0) {
    return 0;
  }
  credit = (adv_want) (marginal * menace * share
                       * tune->attacker_garrison_gain);
  if (credit <= 0) {
    return 0;
  }

  deep_agc_priced++;
  deep_agc_paid += credit;
  log_base(deep_metric_log_level(),
           "deep-agc turn=%d player=\"%s\" city=%d type=\"%s\" wave=%d"
           " marg=%.1f menace=%.3f share=%.3f surv=%.3f credit=%.1f",
           game.info.turn, player_name(pplayer), pcity->id,
           utype_rule_name(ut), now.wave, marginal, menace, share,
           now.surv, (double) credit);

  return credit;
}

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
    unsigned int barb = deep_barbarian_fear(pcity);
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
    deep_apply_garrison_floor(nmap, pcity, city_data, updisc,
                              deep_garrison_menace(pplayer), FALSE);

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
        double priced = 0.0;
        /* The dividend rides on top of whichever tier this branch
         * settles on: it is a value the FIRST defender also earns,
         * and an empty city is precisely where the marginal blade
         * imposes the most attrition. */
        adv_want div
          = deep_deterrence_dividend(nmap, pcity,
                                     choice->type == CT_DEFENDER
                                     ? choice->value.utype : NULL,
                                     deep_garrison_menace(pplayer));

        if (deep_birth_guard_want(ait, nmap, pcity, urgency, &priced)) {
          /* Threat-blind flat 100 replaced by its price. NORMAL tier,
           * so the auction and W34's gates can both see it. */
          deep_want_apply(choice, (struct deep_want) {
            .tier = DWT_NORMAL, .value = priced + div });
        } else {
          deep_want_apply(choice, (struct deep_want) {
            .tier = DWT_EMERGENCY, .value = danger + div });
        }
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
          /* The same dividend on the ordinary defender want: what this
           * body will cost the wave, on top of what it does for the
           * wave's odds. */
          uchoice.want
            += deep_deterrence_dividend(nmap, pcity,
                                        uchoice.type == CT_DEFENDER
                                        ? uchoice.value.utype : NULL,
                                        deep_garrison_menace(pplayer));
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

    /* Still a presence count (military audit C7), deliberately: this
     * and the veteran guard below are the two remaining ones, they
     * both sit on the GARRISON BUILD ladder, and swapping the test
     * for a survival threshold moves how much army a quiet city
     * buys -- which is G1's floor sizing, not this round's. */
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
        /* The two embassy channels, and which one owns this player.
         * With the treaty knobs open, an AI we are at peace-or-better
         * with can be ASKED for an embassy in a clause that costs
         * nothing to build and nothing to walk (deepdiplomacy.c), so
         * paying a diplomat's shields and turns for the same discount
         * is paying twice. The diplomat channel keeps everyone else:
         * players at war, players never met, and every human, none of
         * whom a clause can be relied on to reach. Answers FALSE with
         * the knobs shut, so this want is untouched in the off arm. */
        if (deep_embassy_by_treaty(ait, pplayer, aplayer)) {
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
    /* The second never-launches pump (CONSEQ item 5, derby N3):
     * Gallia switched to Diplomat 191 times for +2 embassies. The
     * reachability gate above already retired the "unsatisfiable want
     * pumps units forever" half of the disease -- an embassy nobody
     * can walk to no longer bids. What it does not answer is the
     * SATURATION half, which is the explorer divisor's own question:
     * a prize that N agents can collect is not worth an N+1th agent.
     *
     * So the diplomat want is divided by the diplomat-class units this
     * player already has walking, plus the ones already in production
     * -- the identical (prize)/(1 + holders + queued) shape deepexplore
     * uses, and no success-rate estimate, which would need an outcome
     * signal nobody has measured yet. 0 = off = exactly 1.0. */
    if (dip->want > 0 && deep_tuning_get()->diplomat_pump_damper > 0.0) {
      int agents = 0;

      unit_list_iterate(pplayer->units, u) {
        if (unit_has_type_flag(u, UTYF_DIPLOMAT)) {
          agents++;
        }
      } unit_list_iterate_end;
      city_list_iterate(pplayer->cities, ocity) {
        if (VUT_UTYPE == ocity->production.kind
            && utype_has_flag(ocity->production.value.utype,
                              UTYF_DIPLOMAT)) {
          agents++;
        }
      } city_list_iterate_end;
      if (agents > 0) {
        double damp = 1.0
          / (1.0 + deep_tuning_get()->diplomat_pump_damper * agents);

        log_base(deep_metric_log_level(),
                 "deep-damper turn=%d player=\"%s\" city=%d pump=diplomat"
                 " agents=%d damp=%.3f want=" ADV_WANT_PRINTF,
                 game.info.turn, player_name(pplayer), pcity->id, agents,
                 damp, dip->want);
        dip->want = (adv_want) (dip->want * damp);
      }
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

          if (btt->transport_capacity > 0 && deep_utype_is_sea(btt)) {
            berths += btt->transport_capacity;
          }
        } unit_list_iterate_end;
        if (berths < needed) {
          lift_demand = MAX(lift_demand, worth);
        }
      }
    }

    /* THE COMMISSION (EXPANSION slice 2). The want above is a site
     * SCORE damped by a surplus: it says "somewhere overseas is worth
     * having, and the fleet is not obviously idle". It cannot say how
     * many founders are standing on the quay, and it exists only on
     * the turns a lift-limited settler search happened to publish
     * colony_want -- which is why the queue can be dry and the
     * shipyard silent in the same turn (measured: 4 of 7 water-turns
     * with a positive gap on serviceable water carried no fresh
     * publication at all).
     *
     * The circle this breaks, all three legs measured:
     *   - slice 1: from t20 the land search scores nothing, so every
     *     founder want left is overseas and the lift is the channel;
     *   - W36: the overseas candidate is discounted by the wait its
     *     water is holding;
     *   - W36-DIM: 86% of that discount's firings land on the
     *     build-side founder want. Demand is damped by the queue, and
     *     the queue exists because demand never commissions supply.
     *
     * So price the hull by what it UNBLOCKS: the settlements already
     * queued behind the shortage, times the delay it takes off them.
     * Every term comes from something already computed --
     *
     *   value   dp->colony_want, deep's own overseas site score, AGED
     *           by amortize() rather than cut off by a freshness
     *           window. A site does not stop being worth what it was
     *           worth because the probe ran three turns ago, and the
     *           same discounter prices every other delay in this file.
     *   n       berths the gap can fill: MIN(gap, this hull's
     *           capacity), both ruleset facts.
     *   gain    value - amortize(value, wait): the delay the hull
     *           removes, in the currency of the thing delayed. Zero
     *           when the water has shown no queue, which is exactly
     *           when deep_wait_expected() refuses to claim a wait.
     *
     * Nothing here is a constant except the weight, which is the
     * on/off switch and defaults to 0.
     *
     * The gate is load-bearing rather than decorative here. The gap
     * census reads 65 positive gaps in 600 water-turns on
     * isolated-150 and only 7 of them on water that BRIDGES: the rest
     * are founders queued beside a sea that closes on its own
     * continent, where no hull can help and boat_water_gate refuses
     * the keel. With the gate off this term would commission for
     * those queues. */
    if (deep_tuning_get()->hull_commission_weight > 0.0
        && dp->colony_want > 0
        && is_terrain_class_near_tile(nmap, ptile, TC_OCEAN)) {
      struct unit_type *boattype
        = best_role_unit_for_player(pplayer, L_FERRYBOAT);
      bool gated = (deep_tuning_get()->boat_gate_at_bid > 0
                    && deep_tuning_get()->boat_water_gate > 0);
      Continent_id seen[8];
      int nseen = 0;

      if (boattype != NULL
          && boattype->transport_capacity > 0
          && can_city_build_unit_now(nmap, pcity, boattype, RPT_CERTAIN)) {
        adjc_iterate(nmap, ptile, atile) {
          Continent_id oc = tile_continent(atile);
          int si, building = 0, founders, berths, gap, wait, n;
          adv_want value, gain, cw;
          int bt;

          if (oc >= 0) {
            continue;                   /* Land. */
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
          if (gated && !deep_water_bridges(oc)) {
            continue;           /* A keel here reaches no other shore. */
          }
          founders = deep_founder_demand_on(ait, pplayer, oc);
          berths = deep_lift_berths_on(ait, pplayer, oc, &building);
          /* ... and the orders taken since the queue was counted.
           * Production is a once-a-turn photograph: every coastal city
           * on this water evaluates against the same gap in the same
           * turn, and a hull ordered one city ago is invisible to
           * `building` until next turn. Measured on the first liveness
           * arm: 8 cities priced one gap of 2 on turn 88 and 5 of them
           * beat their incumbent choice. Orders and not deliveries, so
           * a choice later overridden is still charged -- the
           * conservative direction for a term that multiplies. */
          if (dp->commission_turn != game.info.turn) {
            dp->commission_turn = game.info.turn;
            dp->commission_berths = 0;
          }
          gap = founders - berths - building - dp->commission_berths;
          if (gap <= 0) {
            continue;           /* The queue is already covered. */
          }
          wait = deep_wait_expected(ait, pplayer, oc);
          if (wait <= 0) {
            continue;           /* No queue observed: no delay to buy off. */
          }
          value = amortize(dp->colony_want,
                           MAX(0, game.info.turn - dp->colony_turn));
          n = MIN(gap, boattype->transport_capacity);
          gain = value - amortize(value, wait);
          bt = utype_build_shield_cost(pcity, NULL, boattype)
               / MAX(pcity->surplus[O_SHIELD], 1);
          cw = amortize(deep_tuning_get()->hull_commission_weight * n * gain,
                        MAX(1, bt));
          {
            /* The same appetite gate the colony demand pays: a
             * settler that cannot cross is not an expansion, and the
             * hull it needs is expansion spending. */
            double emult = deep_stance_expand_mult(pplayer);

            if (emult != 1.0 && cw > 0) {
              cw = cw * emult;
              dp->stance_expand_gated++;
            }
          }
          log_base(deep_metric_log_level(),
                   "deep-commission turn=%d player=\"%s\" city=%d"
                   " ocean=%d founders=%d berths=%d building=%d"
                   " ordered=%d gap=%d"
                   " wait=%d age=%d value=" ADV_WANT_PRINTF
                   " n=%d want=" ADV_WANT_PRINTF " beat=%d",
                   game.info.turn, player_name(pplayer), pcity->id, oc,
                   founders, berths, building, dp->commission_berths, gap,
                   wait, MAX(0, game.info.turn - dp->colony_turn), value, n,
                   cw, cw > choice->want ? 1 : 0);
          if (cw > choice->want) {
            choice->value.utype = boattype;
            choice->want = cw;
            choice->type = CT_ATTACKER;
            adv_choice_set_use(choice, "hull commission");
            dp->commission_berths += boattype->transport_capacity;
          }
        } adjc_iterate_end;
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
    /* Presence count, left alone with the severe-danger cutoff above
     * and for the same reason (C7): it is a build-ladder gate that
     * G1 owns. */
    if (floor_def >= MAX(deep_tuning_get()->garrison_floor, 1)
        && city_data->grave_danger == 0) {
      adjust_ai_unit_choice(pcity, choice);
    }
  }

  /* SUCCESSOR-IMMINENT DEFERRAL (CONSEQ item 4, human 2026-08-05:
   * "some units and buildings expire too"). If the unit type that
   * OBSOLETES this choice becomes buildable, on our own research, well
   * inside the time it would take to build this one, the shields are
   * better spent on anything else for those turns -- the interim goes
   * to the normal auction, where HEDGE's liquid caravan is the natural
   * winner once it lands (the coupling is noted, not depended on).
   *
   * TIER DISCIPLINE, and it is the whole guard: never on an emergency
   * want and never in a city carrying danger or urgency. A threatened
   * city buys the body it can have TONIGHT, and a deferral that can
   * cost a city is not an optimisation. Hysteresis via
   * expiry_unit_gap so a wobble in the bulb rate cannot churn the
   * queue between the two answers turn after turn. */
  if (deep_tuning_get()->expiry_unit_gain > 0.0
      && choice->want > 0
      && (choice->type == CT_ATTACKER || choice->type == CT_DEFENDER)
      && choice->value.utype != NULL
      && deep_want_from_choice(choice).tier == DWT_NORMAL
      && city_data->danger == 0 && city_data->urgency == 0) {
    const struct unit_type *next = NULL;
    int eta = deep_utype_successor_eta(pplayer, choice->value.utype,
                                       &next);

    if (eta >= 0 && next != NULL) {
      struct universal tgt = { .kind = VUT_UTYPE,
                               .value.utype = choice->value.utype };
      int build = city_turns_to_build(pcity, &tgt, TRUE);

      if (build > 0 && build < FC_INFINITY
          && eta + deep_tuning_get()->expiry_unit_gap <= build) {
        struct deep_want w = deep_want_from_choice(choice);

        log_base(deep_metric_log_level(),
                 "deep-expiry turn=%d player=\"%s\" city=%d unit=\"%s\""
                 " next=\"%s\" eta=%d build=%d gap=%d want=%.1f",
                 game.info.turn, player_name(pplayer), pcity->id,
                 utype_rule_name(choice->value.utype),
                 utype_rule_name(next), eta, build,
                 deep_tuning_get()->expiry_unit_gap, (double) w.value);
        w.value *= 1.0 - deep_tuning_get()->expiry_unit_gain;
        deep_want_apply(choice, w);
      }
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
  int n_pool = 0, n_elsewhere = 0, n_far = 0, eta, rate, csize;
  struct unit *vunit;
  adv_want tworth, raw_worth, want, gross;
  double l0 = 0.0, l1 = 0.0, loss;
  bool overseas;

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
       * no-fear-before-the-threat law.
       *
       * Barbarians are exempt from the contact requirement, exactly
       * as deep_op_pick_target() exempts them (deepops.c) and as the
       * survival wave does (deepcombat.c): they never make contact,
       * so requiring it means the build side can never fund a unit
       * for a target the ops side forms operations against. That is
       * the disagreement the ops-side comment claimed did not exist
       * (military audit C4), and post-T160 -- 47 empty barbarian
       * cities -- it is most of the target menu. */
      if (!pplayers_at_war(pplayer, aplayer)
          || (!is_barbarian(aplayer)
              && player_diplstate_get(pplayer, aplayer)->type
                 == DS_NO_CONTACT)) {
        continue;
      }
      city_list_iterate(aplayer->cities, acity) {
        double score;
        /* Named apart from the function-scope `overseas`, which is the
         * same predicate about the CHOSEN target rather than about this
         * candidate. */
        bool cand_overseas = tile_continent(city_tile(acity)) != home;

        if (!map_is_known(city_tile(acity), pplayer)
            || (cand_overseas
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
  /* Conquest is worth twice what the city is worth to us, near enough
   * (human call 2026-08-06): taking it BOTH gains us its amortized
   * value AND denies that value to the opponent, and the want has only
   * ever counted the first half. conquest_worth_mult is that second
   * half, applied ONCE here and once at the operation's own net-value
   * gate -- the same referee at both conquest seats, never compounded.
   *
   * Two things this does not fix, both recorded in STATUS: the
   * DEFENSIVE symmetry (keeping our own city is ~2x by the same
   * argument, and multiplying only conquest tilts the offence/defence
   * budget) is parked for the human, and the base being multiplied is
   * still stock's omniscient read of an enemy city (census finding
   * #4). W04's estimator replaces the base; the multiplier survives
   * that swap unchanged.
   *
   * BALANCE adds the second component of the same worth -- the
   * culture stock a captured city carries across -- inside
   * deep_conquest_worth(), so all three conquest seats read one
   * function rather than three copies of a product. */
  raw_worth = def_ai_city_data(target, ait)->worth;
  tworth = deep_conquest_worth(ait, pplayer, target);

  /* Is this want an INVASION want? The chosen target sits on another
   * landmass, so buying this blade is a vote to put an army across
   * water. Hoisted to function scope because it is now also a logged
   * fact: see the deep-kill line at the foot of this function. */
  overseas = tile_continent(city_tile(target)) != home;

  {
    /* March or sail time is the defender's budget on BOTH terrains
     * (one law): the marginal odds are priced against the garrison
     * as it will be at arrival. */
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

  /* The pool that will actually fight for THIS target: own attackers
   * on this continent, not tied to home defence, not committed to a
   * different intention, and CLOSE ENOUGH TO BE THERE.
   *
   * Counting every free attacker as one stream across concurrent
   * operations priced the marginal blade against a force that never
   * assembles -- with four ops running, p1 - p0 goes to zero and the
   * build side stops funding attackers exactly when the campaigns
   * need them (military audit C4). D1 fixed the first half by
   * excluding units an operation has claimed for somewhere else.
   * This is the second half, and it is the same law the odds already
   * run on: the marginal blade is priced at ITS arrival turn, so a
   * free unit that cannot be at the target by then is no more part of
   * that wave than a unit that does not exist. Nothing is invented --
   * the bar is eta, computed above, in the currency the take odds are
   * already taken in.
   *
   * Members of the op aimed at THIS city are exempt: they are
   * committed to arrive whenever they arrive, which is what being the
   * wave means. */
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
    if (deep_op_claimed_elsewhere(pplayer, punit->id, target->id)) {
      n_elsewhere++;
      continue;
    }
    if (tune->assault_pool_reach
        && deep_op_claiming(pplayer, punit->id) < 0) {
      int urate = MAX(tile_continent(city_tile(target)) != home
                      ? rate : unit_type_get(punit)->move_rate, 1);
      int uturns = (real_map_distance(unit_tile(punit),
                                      city_tile(target)) * SINGLE_MOVE
                    + urate - 1) / urate
                   + (tile_continent(city_tile(target)) != home ? 2 : 0);

      if (uturns > eta) {
        n_far++;
        continue;
      }
    }
    pool[n_pool++] = punit;
  } unit_list_iterate_end;

  vunit = unit_virtual_create(pplayer, pcity, punittype, 1);
  p0 = n_pool > 0
       ? deep_take_odds_cost(nmap, target, pool, n_pool, eta, &l0, NULL)
       : 0.0;
  pool[n_pool] = vunit;
  p1 = deep_take_odds_cost(nmap, target, pool, n_pool + 1, eta, &l1, NULL);
  unit_virtual_destroy(vunit);
  marginal = p1 - p0;
  loss = l1 - l0;

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
      double pk, lk;

      virts[made] = unit_virtual_create(pplayer, pcity, punittype, 1);
      pool[n_pool + made] = virts[made];
      made++;
      pk = deep_take_odds_cost(nmap, target, pool, n_pool + made, eta,
                               &lk, NULL);
      if (pk >= tune->op_take_target) {
        if ((pk - p0) / made > marginal) {
          /* The campaign rate replaces the marginal one, and its cost
           * comes with it: a member of the smallest sufficient force
           * is worth the average odds gain and is charged the average
           * loss the campaign runs up. Mixing one's benefit with the
           * other's price would be the cheapest possible attack. */
          marginal = (pk - p0) / made;
          loss = (lk - l0) / made;
        }
        csize = made;
        break;
      }
    }
    while (made > 0) {
      unit_virtual_destroy(virts[--made]);
    }
  }

  /* Cost of victory (military audit G1, mechanism-map coupling 1).
   * The want has always been worth x Dp with no losses term: deep
   * could answer "will I take it" and never "is it worth what it
   * costs". The substrate now reports what the stream is expected to
   * lose, and the marginal blade is charged the marginal loss its
   * presence adds -- the same difference form as the benefit, so a
   * unit that shields the wave is charged less than one that only
   * feeds the walls, and a unit that reduces losses is charged
   * nothing at all.
   *
   * The currency is shields at SHIELD_WEIGHTING, the codebase's own
   * shields-to-want rate (advbuilding.h) and the one HEDGE's switch
   * debit already uses -- so this is the same money the granary is
   * bid in, which is what "same currency" in G1 means. The gain is
   * the referee.
   *
   * Charged BEFORE the amortization, not after: the losses are paid
   * at the same moment the odds are won, so both sides of the trade
   * discount together.
   *
   * CLAMPED AT ZERO, and the clamp is the whole design decision.
   * The marginal loss is NEGATIVE on 49% of the evaluations this
   * session censused (lategame-80, 63 375 lines): the stream strikes
   * strongest-first, so inserting a strong blade at the head often
   * kills the defender in the first duel and the weaker pool members
   * behind it never fight at all. Paying for that would credit this
   * build with the same causal event Dp already credits it with --
   * the defender dying sooner -- and the two together are one effect
   * counted twice. So the term is a COST and never a subsidy: it can
   * only make deep more selective than the pre-E2 want, never more
   * aggressive. The negative branch is real and is recorded as a
   * finding, not spent. */
  gross = tune->attack_value_weight * tworth * marginal;
  {
    adv_want cost = (adv_want) (tune->attack_loss_gain * MAX(loss, 0.0)
                                * SHIELD_WEIGHTING);

    want = military_amortize(pplayer, pcity, MAX(gross - cost, 0),
                             MAX(1, eta), build_cost_balanced(punittype));
  }

  if (tune->op_take_target > 0.0 && p1 >= tune->op_take_target) {
    /* Snap-up sweep (human doctrine, T160: "build quick snap-up
     * units to steal their cities back"): one unit of the proposed
     * type takes this target alone, so the build is a price/speed
     * race, not a power race -- stock's attacker proposal is
     * power-biased. Sweep the buildable land attackers for the type
     * whose amortized want is best: cheap and fast wins on its own
     * arithmetic, no extra weighting. */
    unit_type_iterate(ut) {
      struct unit *v2;
      double pa, la = 0.0;
      adv_want w2;
      int rate2, eta2;

      if (ut == punittype
          || ut->attack_strength <= 0
          || utype_has_flag(ut, UTYF_CIVILIAN)
          || !deep_utype_is_land(ut)
          || !can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
        continue;
      }
      rate2 = MAX(overseas ? rate : ut->move_rate, 1);
      eta2 = (real_map_distance(city_tile(pcity), city_tile(target))
              * SINGLE_MOVE + rate2 - 1) / rate2 + (overseas ? 2 : 0);
      v2 = unit_virtual_create(pplayer, pcity, ut, 1);
      pool[n_pool] = v2;
      pa = deep_take_odds_cost(nmap, target, pool, n_pool + 1, eta2,
                               &la, NULL);
      unit_virtual_destroy(v2);
      if (pa < tune->op_take_target) {
        continue;               /* Too light to snap it up alone. */
      }
      /* Priced net, exactly as the main line is: the race is between
       * types, and a cheap fast one that dies is not a bargain. */
      w2 = military_amortize(pplayer, pcity,
                             MAX(tune->attack_value_weight * tworth
                                 * (pa - p0)
                                 - (adv_want) (tune->attack_loss_gain
                                               * MAX(la - l0, 0.0)
                                               * SHIELD_WEIGHTING), 0),
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

  /* THE NEVER-LAUNCHES DAMPER (CONSEQ item 5, derby N3). No loop
   * existed from "the op never launches" back to "stop building its
   * passengers": derby 3's Iceland built 57 Marines against 6 launches
   * in 600 turns. The explorer pump had the same disease and the same
   * cure -- a saturation divisor keyed to the prize the supply can
   * actually consume -- so this is that shape on a second pump.
   *
   * The quantity is a RATE, not a count: assault operations formed
   * that never reached MARCH, over all formed. Cumulative counts would
   * grow without bound over a long game and turn the damper into a
   * clock. At 0 unlaunched the divisor is exactly 1 and nothing moves;
   * with every op stuck in muster it is 1/(1 + gain). Both counters are
   * saved, because these two drive a decision rather than a log line.
   *
   * Deliberately NOT applied to a threatening target: the same
   * exemption gate 5 takes above. A damper that starves reconquest of
   * what is besieging us is the wrong loop closed. */
  {
    const struct deep_plr *dp = deep_plr_data(pplayer);

    if (tune->attacker_hold_damper > 0.0 && dp->ops_formed > 0
        && dp->ops_launched < dp->ops_formed
        && !deep_op_target_threatens(pplayer, target)) {
      double stuck = (double) (dp->ops_formed - dp->ops_launched)
                     / dp->ops_formed;
      double damp = 1.0 / (1.0 + tune->attacker_hold_damper * stuck);

      log_base(deep_metric_log_level(),
               "deep-damper turn=%d player=\"%s\" city=%d pump=attacker"
               " formed=%d launched=%d stuck=%.3f damp=%.3f"
               " want=" ADV_WANT_PRINTF,
               game.info.turn, player_name(pplayer), pcity->id,
               dp->ops_formed, dp->ops_launched, stuck, damp, want);
      want = (adv_want) (want * damp);
    }
  }

  /* THE GARRISON CREDIT (NETTHREAT item 3). What this same body is
   * worth to the city that builds it on the nights it is standing
   * here. The bookkeeping rule that keeps it disjoint from the
   * deterrence dividend and from the sortie prize is stated in full at
   * deep_attacker_garrison_credit(). */
  want += deep_attacker_garrison_credit(nmap, pcity, punittype,
                                        deep_garrison_menace(pplayer));

  /* sea= is the invasion want's own counter, and eta= is the budget it
   * gave the defender. Without them no archived corpus can answer "does
   * deep ever want to INVADE" -- the naval half of this want has always
   * been priced (overseas targets joined the menu with naval slice 3)
   * and has never been separable on the line from a land want, which is
   * the evidence gap the convoy family's excluded-dead verdict rests
   * on. Both are facts already computed above; nothing is decided here. */
  log_base(deep_metric_log_level(),
           "deep-kill turn=%d player=\"%s\" city=%d target=%d barb=%d"
           " sea=%d eta=%d"
           " worth=" ADV_WANT_PRINTF " eworth=" ADV_WANT_PRINTF
           " pool=%d elsewhere=%d far=%d p0=%.3f"
           " p1=%.3f ck=%d type=\"%s\" gross=" ADV_WANT_PRINTF
           " loss=%.2f want=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), pcity->id, target->id,
           is_barbarian(city_owner(target)) ? 1 : 0,
           overseas ? 1 : 0, eta,
           raw_worth, tworth, n_pool, n_elsewhere, n_far, p0, p1,
           csize,
           utype_rule_name(punittype), gross, loss, want);

  if (want > choice->want) {
    choice->value.utype = punittype;
    choice->want = want;
    choice->type = CT_ATTACKER;
    adv_choice_set_use(choice, "deep assault");
  }
}

struct deep_emergency {
  struct city *pcity;
  double surv;
  unsigned int urgency;
  int capital;
  adv_want worth;
  int id;
};

/**********************************************************************//**
  Severity order for simultaneous city emergencies: how likely the city
  is to fall first, then how close the danger is, then strategic role,
  then plain city value; city id keeps the sort deterministic.
  Lexicographic on purpose -- no scale-mixing weights to invent.

  The first key used to be grave_danger, a presence count that
  increments for any capture-capable unit within one turn's move and
  asks nothing about whether the garrison can hold (the project's own
  recurring defect; instance six of nine in the military audit's list,
  C7). It decides which of two dying cities gets the gold, so it is
  the one that most deserves the probability. The verdict is the
  shared one D2 landed -- computed once per city per phase and read
  here, not recomputed against a map that has moved since the citizen
  arrangement asked the same question.
**************************************************************************/
static int deep_emergency_cmp(const void *a_, const void *b_)
{
  const struct deep_emergency *a = a_, *b = b_;

  if (a->surv != b->surv) {
    return a->surv < b->surv ? -1 : 1;   /* Least likely to hold first. */
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

/* Rung 1's census, per player per turn. */
static int deep_sd_short = 0, deep_sd_sold = 0, deep_sd_bought = 0;
static int deep_sd_gold = 0;   /* sale proceeds raised this phase */
static int deep_late_mark = -1; /* treasury the main buy pass left */

/**********************************************************************//**
  RUNG 1: SELL TO DEFEND (human design, 2026-08-07 -- "sell a bldg then
  buy defense unit as a lastditch").

  A DEFENSIVE move, not a concession, and that is what separates it
  from the abandonment rung: the building is spent to KEEP the city,
  not written off with it. The misfire cost is one building against a
  city that turns out not to have needed saving, so the trigger is
  deliberately LOOSER than the abandonment slice's -- a defender is
  wanted, the treasury cannot cover the rush, and the write-off
  family's own danger read says the city is below survival target
  against a wave that exists.

  Stock has a sibling of this and it is far narrower: it fires only
  when grave_danger is set AND the city has literally no defence, and
  it sells through try_to_sell_stuff(). This rung fires on the
  SHORTFALL, which is the condition that actually predicts a city
  bought nothing.

  WHAT IT SELLS: the highest-value sellable building, minus the ones
  that are themselves defending this city. Selling City Walls to buy
  the Musketeer standing behind them is a trade the city can lose on
  net, and it is the reason stock spares them too; the exclusion is
  stated here rather than hidden so it can be reversed by whoever
  disagrees. Great and small wonders are not sellable at all
  (is_building_sellable() is is_improvement()), so they never enter.

  ONE SALE PER CITY PER TURN is the server's rule (`did_sell`,
  server/cityhand.c), honoured explicitly the way deep_balance_treasury()
  does.

  TIMING, stated because it bounds what this rung can do: the bought
  unit arrives at turn change, so this saves a city from NEXT turn's
  wave. An attack landing this turn is rung 2's problem, and rung 2 is
  census-and-design in this session.

  sell_defend_gain 0 = off, and the branch returns FALSE before
  reading anything.
**************************************************************************/
static bool deep_sell_to_defend(struct ai_type *ait, struct player *pplayer,
                                struct city *pcity,
                                const struct adv_choice *bestchoice,
                                int buycost, int expenses)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct impr_type *best = NULL;
  int best_gold = 0, wave = 0, shortfall;
  double surv;

  if (tune->sell_defend_gain <= 0.0
      || bestchoice->type != CT_DEFENDER
      || pcity->did_sell
      || buycost <= 0
      || pplayer->economic.gold - expenses >= buycost) {
    return FALSE;               /* No shortfall to close. */
  }

  /* The write-off family's own read, and the same call every other
   * consumer of it makes. */
  surv = deep_city_survival_verdict(nmap, pcity, tune->survival_horizon,
                                    &wave);
  if (wave == 0 || surv >= tune->survival_target) {
    return FALSE;
  }
  deep_sd_short++;
  shortfall = buycost - (pplayer->economic.gold - expenses);

  /* The gain is the BAR, not a scale: sell only when the building we
   * would give up is worth less than the defender is, in the one
   * currency both are quoted in here -- gold. */
  city_built_iterate(pcity, pimprove) {
    int gold;

    if (!can_city_sell_building(pcity, pimprove)
        || building_has_effect(pimprove, EFT_DEFEND_BONUS)) {
      continue;
    }
    gold = impr_sell_gold(pimprove);
    if (gold > best_gold) {
      best_gold = gold;
      best = (struct impr_type *) pimprove;
    }
  } city_built_iterate_end;

  if (best == NULL || best_gold <= 0
      || pplayer->economic.gold + best_gold - expenses < buycost
      || best_gold > tune->sell_defend_gain * buycost) {
    log_base(deep_metric_log_level(),
             "deep-selldefend turn=%d player=\"%s\" city=%d surv=%.3f"
             " wave=%d shortfall=%d sold=\"-\" gold=%d bought=0",
             game.info.turn, player_name(pplayer), pcity->id, surv, wave,
             shortfall, best_gold);
    return FALSE;
  }

  do_sell_building(pplayer, pcity, best, "sold");
  pcity->did_sell = TRUE;
  deep_sd_sold++;
  deep_sd_gold += best_gold;

  {
    bool bought = pplayer->economic.gold - expenses >= buycost;

    if (bought) {
      really_handle_city_buy(pplayer, pcity);
      deep_sd_bought++;
    }
    log_base(deep_metric_log_level(),
             "deep-selldefend turn=%d player=\"%s\" city=%d surv=%.3f"
             " wave=%d shortfall=%d sold=\"%s\" gold=%d bought=%d"
             " unit=\"%s\"",
             game.info.turn, player_name(pplayer), pcity->id, surv, wave,
             shortfall, improvement_rule_name(best), best_gold,
             bought ? 1 : 0,
             utype_rule_name(bestchoice->value.utype));
  }

  return TRUE;
}

/**********************************************************************//**
  RUNG 3: THE FINAL DEFENCE BUY (human addition, 2026-08-07).

  The main buy pass runs at a fixed point inside the phase, and the
  phase keeps earning after it. Money that arrives later waits a whole
  turn that an endangered city may not have.

  WHERE THIS ACTUALLY SITS, said plainly because the honest seam
  matters more than the label: the last statement of
  deep_do_last_activities(), which is the final thing deep does in its
  own phase -- after deep_spend_gold(), after deep_balance_treasury(),
  after dai_manage_tech(). It is NOT a server phase_finished hook;
  that callback is a data-lifecycle hook called from many places
  including deep's own reads of another player's ai_plr, and hanging a
  spending action on it would fire it at times that have nothing to do
  with the end of a turn.

  What that placement means for the money it can find, in today's
  ordering: rung 1's own sale is spent inside deep_spend_gold() the
  moment it happens, so this pass is not what banks it; what it does
  catch is any income the server credits between the two points, the
  tech pass's refunds, and -- the durable reason to have it -- any
  future reordering that moves an earner after the buy. The counter
  says how much it actually found, so the claim is checkable rather
  than argued.

  BUDGET: strictly the gold ADDED since the main pass left the
  treasury. deep_balance_treasury() sells buildings to stay solvent,
  and spending that would undo the guard that raised it, so the
  watermark is taken AFTER the treasury guard runs.

  POPULATION: triage only -- urgency, grave danger, or the write-off
  family's own below-target survival against a live wave. Never the
  general want ladder; a late pass that could buy a Granary is a
  second spending policy, not an emergency valve.

  late_buy_gate 0 = off, and the pass returns before reading a city.
**************************************************************************/
static void deep_late_defence_buy(struct ai_type *ait,
                                  struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int budget, spent = 0, bought = 0, cands = 0;

  if (tune->late_buy_gate <= 0 || deep_late_mark < 0) {
    deep_late_mark = -1;
    return;
  }
  budget = pplayer->economic.gold - deep_late_mark;
  deep_late_mark = -1;
  if (budget <= 0) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    int buycost, wave = 0;
    double surv = 1.0;

    if (VUT_UTYPE != pcity->production.kind) {
      continue;                 /* An emergency buys a body. */
    }
    if (city_data->urgency == 0 && city_data->grave_danger == 0) {
      surv = deep_city_survival_verdict(nmap, pcity,
                                        tune->survival_horizon, &wave);
      if (wave == 0 || surv >= tune->survival_target) {
        continue;
      }
    }
    cands++;
    buycost = city_production_buy_gold_cost(pcity);
    if (buycost <= 0 || buycost > budget - spent) {
      continue;
    }
    /* Never spend the empire into the reaper the treasury guard just
     * pulled it out of. */
    if (player_get_expected_income(pplayer)
        < -(pplayer->economic.gold - buycost)) {
      continue;
    }
    really_handle_city_buy(pplayer, pcity);
    spent += buycost;
    bought++;
    log_base(deep_metric_log_level(),
             "deep-latebuy turn=%d player=\"%s\" city=%d unit=\"%s\""
             " cost=%d added=%d fromsale=%d urgency=%u grave=%d"
             " surv=%.3f",
             game.info.turn, player_name(pplayer), pcity->id,
             utype_rule_name(pcity->production.value.utype), buycost,
             budget, MIN(deep_sd_gold, budget), city_data->urgency,
             city_data->grave_danger != 0 ? 1 : 0, surv);
  } city_list_iterate_end;

  if (cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-latebuy-sum turn=%d player=\"%s\" triage=%d added=%d"
             " fromsale=%d bought=%d spent=%d",
             game.info.turn, player_name(pplayer), cands, budget,
             MIN(deep_sd_gold, budget), bought, spent);
  }
}

/**********************************************************************//**
  Rung 1's execution counter, one line per player per turn.
**************************************************************************/
static void deep_sell_defend_census(struct player *pplayer)
{
  if (deep_sd_short > 0) {
    log_base(deep_metric_log_level(),
             "deep-selldefend-sum turn=%d player=\"%s\" shortfalls=%d"
             " sold=%d bought=%d",
             game.info.turn, player_name(pplayer), deep_sd_short,
             deep_sd_sold, deep_sd_bought);
  }
  deep_sd_short = 0;
  deep_sd_sold = 0;
  deep_sd_bought = 0;
  deep_sd_gold = 0;
}

/**********************************************************************//**
  Buy and upgrade stuff. Derived from dai_spend_gold(); the difference
  is what happens to explorers standing in a city. Stock scraps every
  one that returns home, which in hut games destroys the units the new
  explorer want just paid for; deep salvages only the ones the visible
  prize can no longer employ (deep_explore_surplus()).
**************************************************************************/
static void deep_spend_gold(struct ai_type *ait, struct player *pplayer)
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

  {
    /* Salvage, not scrapping. Stock disbands every explorer that walks
     * home; deep's first version gated that on the explore want being
     * zero, and the want was a hyperbola that never reached zero, so
     * the gate never opened and fleets equilibrated at 20-51 units
     * (STATUS M3). deep_explore_surplus() closes the loop on the same
     * marginal quantity the build want reads, with a hysteresis gap so
     * the two cannot churn against each other and a retention floor so
     * a reopened island (or W04's recon value) still has feet.
     *
     * unit_do_disband_trad() is the convertible-asset form already:
     * it tries Help Wonder (100% of the shields) and then Disband Unit
     * Recover (50%) against the city on the unit's tile, and only
     * falls back to plain disbanding if the ruleset refuses both. The
     * unit therefore has to BE in one of our cities, which is also
     * where an idle explorer ends up. */
    int surplus = deep_explore_surplus(pplayer);
    int salvaged = 0;

    city_list_iterate(pplayer->cities, pcity) {
      struct tile *ptile = pcity->tile;

      if (surplus <= 0) {
        break;
      }
      unit_list_iterate_safe(ptile->units, punit) {
        if (surplus <= 0) {
          break;
        }
        if (unit_has_type_role(punit, L_EXPLORER)
            && unit_owner(punit) == pplayer
            && def_ai_city_data(pcity, ait)->urgency == 0) {
          int before = pcity->shield_stock;

          CITY_LOG(LOG_BUY, pcity, "salvaging %s: fleet above the prize",
                   unit_rule_name(punit));
          unit_do_disband_trad(pplayer, punit, ACT_REQ_PLAYER);
          log_base(deep_metric_log_level(),
                   "deep-salvage turn=%d player=\"%s\" city=%d"
                   " surplus=%d recovered=%d",
                   game.info.turn, player_name(pplayer), pcity->id,
                   surplus, pcity->shield_stock - before);
          surplus--;
          salvaged++;
        }
      } unit_list_iterate_safe_end;
    } city_list_iterate_end;
    (void) salvaged;
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
    if (deep_tuning_get()->buy_per_gold > 0) {
      /* DARKFX: rank by want per GOLD. Buy cost is quadratic in the
       * shields still missing for a unit, so the same treasury buys
       * several times the shields when it tops up a nearly finished
       * build; want alone cannot see that, because want belongs to the
       * thing and the discount belongs to its progress.
       *
       * The triage keeps its own tier and its own ordering: a city
       * whose urgency or grave_danger says it may fall this turn is
       * ranked on want exactly as before and beats every bargain in
       * the empire. A completed production (cost 0) also stays on want
       * so the loop still reaches it and retires it, as before. */
      double bestkey = 0.0;
      int besttier = -1;

      city_list_iterate(pplayer->cities, acity) {
        struct ai_city *acity_data = def_ai_city_data(acity, ait);
        int cost;
        int tier;
        double key;

        if (acity_data->choice.want <= 0) {
          continue;
        }
        tier = (acity_data->urgency > 1 || acity_data->grave_danger != 0)
               ? 1 : 0;
        cost = city_production_buy_gold_cost(acity);
        key = (tier > 0 || cost <= 0)
              ? (double) acity_data->choice.want
              : (double) acity_data->choice.want / cost;

        if ((tier > besttier || (tier == besttier && key > bestkey))
            && ai_fuzzy(pplayer, TRUE)) {
          besttier = tier;
          bestkey = key;
          adv_choice_copy(&bestchoice, &(acity_data->choice));
          pcity = acity;
        }
      } city_list_iterate_end;
    } else {
      city_list_iterate(pplayer->cities, acity) {
        struct ai_city *acity_data = def_ai_city_data(acity, ait);

        if (acity_data->choice.want
            > bestchoice.want && ai_fuzzy(pplayer, TRUE)) {
          adv_choice_copy(&bestchoice, &(acity_data->choice));
          pcity = acity;
        }
      } city_list_iterate_end;
    }

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
      /* DARKFX's buy census, ungated so the two ranking policies are
       * comparable: what the gold actually bought, in shields. */
      log_base(deep_metric_log_level(),
               "deep-buy turn=%d player=\"%s\" city=%d kind=\"%s\""
               " gold=%d shields=%d want=%.1f urgency=%d tier=\"%s\"",
               game.info.turn, player_name(pplayer), pcity->id,
               adv_choice_rule_name(&bestchoice), buycost,
               MAX(0, city_production_build_shield_cost(pcity)
                      - pcity->shield_stock),
               (double) bestchoice.want, city_data->urgency, "normal");
      really_handle_city_buy(pplayer, pcity);
    } else if (deep_sell_to_defend(ait, pplayer, pcity, &bestchoice,
                                   buycost, expenses)) {
      /* Rung 1 fired and reported. Nothing further to try here. */
      increase_maxbuycost(pplayer, buycost);
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
        log_base(deep_metric_log_level(),
                 "deep-buy turn=%d player=\"%s\" city=%d kind=\"%s\""
                 " gold=%d shields=%d want=%.1f urgency=%d tier=\"%s\"",
                 game.info.turn, player_name(pplayer), pcity->id,
                 adv_choice_rule_name(&bestchoice), buycost,
                 MAX(0, city_production_build_shield_cost(pcity)
                        - pcity->shield_stock),
                 (double) bestchoice.want, city_data->urgency, "sold");
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
  The production class the change penalty charges on, as a one-letter
  tag. Mirrors the switch in city_change_production_penalty()
  (common/city.c:1950) exactly -- U unit, I improvement, W wonder,
  N nothing (the class that is never penalized).
**************************************************************************/
static char deep_prod_class(const struct universal *target)
{
  switch (target->kind) {
  case VUT_IMPROVEMENT:
    return is_wonder(target->value.building) ? 'W' : 'I';
  case VUT_UTYPE:
    return 'U';
  default:
    return 'N';
  }
}

/**********************************************************************//**
  The universal this choice would put into production, or FALSE when
  the choice does not name one.
**************************************************************************/
static bool deep_choice_target(const struct adv_choice *choice,
                               struct universal *target)
{
  switch (choice->type) {
  case CT_CIVILIAN:
  case CT_ATTACKER:
  case CT_DEFENDER:
    if (choice->value.utype == NULL) {
      return FALSE;
    }
    target->kind = VUT_UTYPE;
    target->value.utype = choice->value.utype;
    return TRUE;
  case CT_BUILDING:
    if (choice->value.building == NULL) {
      return FALSE;
    }
    target->kind = VUT_IMPROVEMENT;
    target->value.building = choice->value.building;
    return TRUE;
  default:
    return FALSE;
  }
}

/**********************************************************************//**
  The endgame score mode's UNIT half: inside the final
  `endgame_window` turns, deflate a unit nobody will finish building.

  The improvement half runs earlier, on building_want[] itself
  (deephorizon.c), so this seam deliberately touches VUT_UTYPE targets
  only -- deflating a building here as well would charge it twice for
  the same bell. Between them the whole auction is covered: every
  candidate at this seam is a unit or an improvement.

  The emergency tier is exempt, as every deep gate exempts it. A city
  that needs a defender this turn is not playing for score, and a
  half-built defender that will not arrive is still the only thing
  standing between the city and a capture that costs far more than the
  metric.

  Returns the want removed, or 0 when nothing was.
**************************************************************************/
static adv_want deep_endgame_unit_deflate(struct player *pplayer,
                                          struct city *pcity,
                                          struct adv_choice *choice,
                                          const char *who)
{
  double mult = deep_tuning_get()->endgame_unfinished_gain;
  struct universal target;
  adv_want was;

  if (mult == 1.0
      || !deep_in_endgame_window()
      || choice->want <= 0
      || deep_want_from_choice(choice).tier == DWT_EMERGENCY
      || !deep_choice_target(choice, &target)
      || target.kind != VUT_UTYPE
      || deep_finishes_in_time(pcity, &target)) {
    return 0;
  }

  was = choice->want;
  choice->want = choice->want * mult;

  log_base(deep_metric_log_level(),
           "deep-endgame-unit turn=%d player=\"%s\" city=%d who=\"%s\""
           " pick=\"%s\" left=%d ttb=%d want=" ADV_WANT_PRINTF
           " to=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), pcity->id, who,
           adv_choice_rule_name(choice), deep_turns_left(),
           city_turns_to_build(pcity, &target, TRUE), was, choice->want);

  return was - choice->want;
}

/**********************************************************************//**
  CHARGE A UNIT FOR THE LIFE IT IS GOING TO LIVE (M4b item 5,
  amortization audit 1b).

  A building's gold upkeep is charged against its horizon credit over
  MORT (deephorizon.c, `upkeep_gold_weight`). A unit's upkeep over its
  lifetime is charged NOWHERE at build time. Only the defence FEAR
  terms carry a partial discount (`upkeep_unit_weight`), which is a
  different seam pricing a different question -- how much fear a
  garrison is worth -- and it never reaches the settler, the worker,
  the ferry or the attacker. So a Warriors kept 200 turns costs about
  twenty times its build price and the auction is told it costs the
  build price, which is the asymmetry that makes a cheap unit look
  free forever. Derby-3's birth guards are the standing demonstration.

  Charged here, at the seam where every candidate of every class
  exists and nothing has consumed the bid yet -- the same seam the
  endgame deflator and the switch debit already use, for the same
  reason.

  E[lifetime] is deep_horizon_turns(), the same horizon the building
  side charges over, truncated by CONSEQ's obsolescence ETA where the
  ruleset has a successor on the way: that is the turn deep's own
  rotation melts the unit, so paying upkeep past it would be pricing a
  unit the AI has already decided to replace.

  Two conversions, neither minted here: shield upkeep at
  SHIELD_WEIGHTING (the codebase's own shield-to-want constant, the
  switch debit's too), and gold PLUS FOOD upkeep at
  `upkeep_gold_weight` (the BUILDING side's own knob, whose dimension
  is already "per turn against a horizon"). The 1:1 gold-food
  equivalence is the one deep_defender_upkeep_discount() in this file
  already makes; reusing it is rule 3, minting a second one would be
  rule 7.

  Food is in the sum because leaving it out made the mechanism empty.
  Measured on contested, one game, every candidate logged: of 321
  unit candidates reaching this test, 281 were Settlers with
  ups=0 upg=0 upf=1, because under the governments deep actually
  plays early -- Despotism, Tribal, Anarchy -- civ2civ3 charges no
  gold and no shields for these units and charges FOOD. A gold-and-
  shield-only charge fired 5 times a game out of 616 evaluations. The
  cost a settler really imposes is the food that suppresses the city
  it came from, and that is now what it is charged.

  Emergency tier exempt, as every deep gate exempts it: a city that
  needs a defender this turn is not shopping on running costs. Note
  the interaction -- with `birth_guard_gain` open, CONSEQ re-prices the
  first-defender want to DWT_NORMAL, which is what brings the derby's
  own birth-guard population INTO this charge's reach.

  Returns the debit, or 0.
**************************************************************************/
static adv_want deep_unit_upkeep_charge(struct player *pplayer,
                                        struct city *pcity,
                                        struct adv_choice *choice,
                                        const char *who)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct universal target;
  const struct unit_type *ut;
  int life, eta, up_shield, up_gold, up_food;
  double charge;
  adv_want debit;

  if (tune->unit_upkeep_charge <= 0.0
      || choice->want <= 0
      || deep_want_from_choice(choice).tier == DWT_EMERGENCY
      || !deep_choice_target(choice, &target)
      || target.kind != VUT_UTYPE) {
    return 0;
  }
  ut = target.value.utype;

  up_shield = utype_upkeep_cost(ut, NULL, pplayer, O_SHIELD);
  up_gold = utype_upkeep_cost(ut, NULL, pplayer, O_GOLD);
  up_food = utype_upkeep_cost(ut, NULL, pplayer, O_FOOD);
  if (up_shield <= 0 && up_gold <= 0 && up_food <= 0) {
    return 0;                   /* Free to keep: nothing owed. */
  }

  life = deep_horizon_turns();
  eta = deep_utype_successor_eta(pplayer, ut, NULL);
  if (eta >= 0 && eta < life) {
    life = eta;
  }
  if (life <= 0) {
    return 0;
  }

  /* Gold and FOOD are summed as one recurring bill and charged at
   * upkeep_gold_weight. That 1:1 gold-food equivalence is not minted
   * here: deep_defender_upkeep_discount() in this same file already
   * sums utype_upkeep_cost(O_GOLD) + utype_upkeep_cost(O_FOOD) into a
   * single "what the marginal defender costs to keep" figure under a
   * single weight. Reusing the module's existing equivalence is rule
   * 3; inventing a second one would be rule 7. A tuner should know
   * the two outputs ride one knob. */
  charge = (double) up_shield * life * SHIELD_WEIGHTING
           + (double) (up_gold + up_food) * life
             * tune->upkeep_gold_weight;
  debit = (adv_want) (charge * tune->unit_upkeep_charge);
  if (debit <= 0) {
    return 0;
  }

  deep_upcharge_n++;
  deep_upcharge_paid += debit;
  log_base(deep_metric_log_level(),
           "deep-upcharge turn=%d player=\"%s\" city=%d who=\"%s\""
           " pick=\"%s\" life=%d eta=%d ups=%d upg=%d upf=%d"
           " debit=" ADV_WANT_PRINTF " want=" ADV_WANT_PRINTF
           " left=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), pcity->id, who,
           adv_choice_rule_name(choice), life, eta, up_shield, up_gold,
           up_food, debit, choice->want, MAX(choice->want - debit, 0));

  choice->want = MAX(choice->want - debit, 0);

  return debit;
}

/**********************************************************************//**
  Debit one candidate by the shields its own production change would
  really forfeit, and return the debit in want.

  The forfeit is `city_change_production_penalty()` against the stock
  the city is actually holding -- the same function the server runs a
  few statements later -- so a candidate in the incumbent's class is
  charged nothing because the ruleset charges nothing for it, a city
  that built last turn is charged only what its surplus does not cover,
  and a city with an empty box is charged nothing at all. Stock's dock
  knows none of those three things: it takes a flat fraction of the
  whole stock whenever the class differs.

  The conversion is SHIELD_WEIGHTING per shield, the codebase's own
  shield-to-want constant (advbuilding.h:20; `adv->shield_priority` is
  set to exactly it, advdata.c:510). No new scale is minted.

  Emergency-tier choices are exempt, as every deep gate exempts them:
  a city that needs a defender this turn is not shopping.
**************************************************************************/
static adv_want deep_switch_cost_apply(struct player *pplayer,
                                       struct city *pcity,
                                       struct adv_choice *choice,
                                       const char *who)
{
  struct universal target;
  adv_want debit;
  int forfeit;

  if (deep_tuning_get()->switch_cost_price <= 0
      || choice->want <= 0
      || deep_want_from_choice(choice).tier == DWT_EMERGENCY
      || !deep_choice_target(choice, &target)
      || are_universals_equal(&target, &pcity->changed_from)) {
    return 0;
  }

  forfeit = pcity->shield_stock - city_change_production_penalty(pcity,
                                                                &target);
  if (forfeit <= 0) {
    return 0;
  }

  debit = (adv_want) (forfeit * SHIELD_WEIGHTING
                      * deep_tuning_get()->switch_cost_gain);

  log_base(deep_metric_log_level(),
           "deep-switchcost turn=%d player=\"%s\" city=%d who=\"%s\""
           " pick=\"%s\" forfeit=%d stock=%d debit=" ADV_WANT_PRINTF
           " want=" ADV_WANT_PRINTF " left=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), pcity->id, who,
           adv_choice_rule_name(choice), forfeit, pcity->shield_stock,
           debit, choice->want, MAX(choice->want - debit, 0));

  choice->want = MAX(choice->want - debit, 0);

  return debit;
}

/**********************************************************************//**
  Census one production change at the seam where deep COMMITS it, with
  the shields the change-production penalty WOULD take.

  It would take them and does not: change_build_target()
  (server/citytools.c:3221) applies the penalty only to humans and to
  AI players carrying H_PRODCHGPEN, and no difficulty level sets that
  flag above `easy`. Deep therefore switches production for free
  today, and stock's own want dock for held shields
  (daicity.c:1909) prices a cost the seat never pays.

  `loss` is what the server would have deducted from THIS stock at
  THIS moment -- the honest per-event figure. Summed over a game it
  is an upper bound on the exempted mass rather than the exact one,
  because under the penalty the stock the next event acts on would
  already be smaller; the census reports both the sum and the event
  count so the bound is visible.

  Logged only when the target differs from what the turn started
  building (`changed_from`), which is the population the penalty acts
  on; a city re-confirming its build every turn is not an event.
**************************************************************************/
static void deep_prodswitch_log(struct ai_type *ait, struct player *pplayer,
                                struct city *pcity,
                                const struct universal *build_new)
{
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  const struct unit_type *carav = NULL;
  int after, loss;

  if (are_universals_equal(build_new, &pcity->changed_from)) {
    return;
  }

  after = city_change_production_penalty(pcity, build_new);
  loss = pcity->shield_stock - after;

  if (num_role_units(action_id_get_role(ACTION_HELP_WONDER)) > 0) {
    carav = best_role_unit(pcity, action_id_get_role(ACTION_HELP_WONDER));
  }

  log_base(deep_metric_log_level(),
           "deep-prodswitch turn=%d player=\"%s\" city=%d"
           " from=\"%s\" to=\"%s\" fromk=%c tok=%c"
           " stock=%d after=%d loss=%d bcs=%d carsh=%d disb=%d lts=%d"
           " builtlast=%d surplus=%d size=%d"
           " danger=%u urgency=%u menace=%.3f"
           " carav=%d carcost=%d coin=%d pen=%d",
           game.info.turn, player_name(pplayer), pcity->id,
           universal_rule_name(&pcity->changed_from),
           universal_rule_name(build_new),
           deep_prod_class(&pcity->changed_from),
           deep_prod_class(build_new),
           pcity->shield_stock, after, loss,
           pcity->before_change_shields, pcity->caravan_shields,
           pcity->disbanded_shields, pcity->last_turns_shield_surplus,
           city_built_last_turn(pcity) ? 1 : 0,
           pcity->surplus[O_SHIELD], city_size_get(pcity),
           city_data->danger, city_data->urgency,
           deep_garrison_menace(pplayer),
           carav != NULL ? 1 : 0,
           carav != NULL ? utype_build_shield_cost(pcity, NULL, carav) : 0,
           (build_new->kind == VUT_IMPROVEMENT
            && is_convert_improvement(build_new->value.building)) ? 1 : 0,
           /* Whether `loss` is about to be REALLY taken, one event at a
            * time: the seat carries the handicap, so the change the
            * next statement makes will run the penalty. */
           has_handicap(pplayer, H_PRODCHGPEN) ? 1 : 0);
}

/**********************************************************************//**
  Census the competing bids BEFORE the auction, at the one seam where
  every class's price for the same city-turn is simultaneously
  readable.

  `deep-want` (below) records the WINNER, which is how the July 2026
  W01 corpus was taken; a winner-conditioned sample cannot say what a
  class bid when it lost, and the classes this project most needs to
  compare -- the founder against the building anchor -- rarely win in
  the same city-turn. R2b's two-price logging is the pattern: both
  prices for the same decision, on the same line.

  Every value here is already computed and none is recomputed:
  `founder_want` and `worker_want` are `ai/default`'s own fields
  (daisettlers.c / daicity.c) as they stand before
  domestic_advisor_choose_build() scales them by traits, by `expand`
  and by the wonder-city divisor; `mil` is deep's military choice,
  formed above; `build` is the same maximum over
  `pcity->server.adv->building_want[]` that building_advisor_choose()
  takes, computed here WITHOUT the AI's choose_building callback so
  the census cannot perturb the decision it is watching.

  `founder` keeps its sign: negative means "I want to found and I need
  a boat first" (daicity.h:60), so its magnitude is the want and its
  sign is the errand.
**************************************************************************/
static void deep_bid_log(struct player *pplayer, struct city *pcity,
                         const struct ai_city *city_data)
{
  struct deep_want mil = deep_want_from_choice(&city_data->choice);
  adv_want build = 0;

  improvement_iterate(pimprove) {
    int id = improvement_index(pimprove);

    if (!is_wonder(pimprove)
        && pcity->server.adv->building_want[id] > build
        && can_city_build_improvement_now(pcity, pimprove, RPT_CERTAIN)) {
      build = pcity->server.adv->building_want[id];
    }
  } improvement_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-bid turn=%d player=\"%s\" city=%d"
           " founder=" ADV_WANT_PRINTF " worker=" ADV_WANT_PRINTF
           " expand=%d mil=" ADV_WANT_PRINTF " miltier=%d"
           " build=" ADV_WANT_PRINTF " size=%d",
           game.info.turn, player_name(pplayer), pcity->id,
           (adv_want) city_data->founder_want,
           (adv_want) city_data->worker_want,
           pplayer->ai_common.expand,
           mil.value, (int) mil.tier, build, city_size_get(pcity));
}

/**********************************************************************//**
  Census what happens to `founder_want` AFTER deep has finished writing
  it and BEFORE the settler is bid for -- the four transforms
  domestic_advisor_choose_build() (daidomestic.c:537-604) applies that
  deep never sees the result of.

  Two of them are divisors that can silently annihilate a want deep
  worked to produce: the wonder-city divisor (/5 or /2) and the
  city-count divisor (/100 when `adv->max_num_cities` is reached or the
  player cannot found at all). One is a trait multiplier. The fourth is
  not a scale at all but a VETO: a city whose food surplus is below the
  founder's food upkeep makes no founder bid whatever, at any want.

  `bid` is the number daidomestic will actually compare against the
  worker/building bid, so `deep-founder want=W` (deepsite.c) minus this
  line's `bid` is exactly what the ai/default layer takes off the top.
  Nothing here recomputes a decision or touches one: every input is a
  field or a pure query, and the function is called from the same seam
  as deep_bid_log(), before the auction.
**************************************************************************/
static void deep_founder_bid_log(struct player *pplayer, struct city *pcity,
                                 const struct ai_city *city_data)
{
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct unit_type *founder_type
    = best_role_unit(pcity, action_id_get_role(ACTION_FOUND_CITY));
  bool wonder = (adv->wonder_city == pcity->id);
  bool capped = (adv->max_num_cities <= city_list_size(pplayer->cities)
                 || !player_can_do_action_result(pplayer,
                                                 ACTRES_FOUND_CITY));
  int trait = ai_trait_get_value(TRAIT_EXPANSIONIST, pplayer);
  int upkeep = (founder_type != NULL
                ? utype_upkeep_cost(founder_type, NULL, pplayer, O_FOOD)
                : 0);
  bool fed = (founder_type != NULL && pcity->surplus[O_FOOD] >= upkeep);
  adv_want bid = (adv_want) city_data->founder_want;
  /* Whether the food the veto measures is under deep's management at
   * all. `deep_set_city_manager_params()` sets cm_parameter only where
   * a posture matched and frees it otherwise, so cm=0 means this city's
   * citizens are arranged by the stock default and no deep rule asked
   * for the surplus that the founder bid needs. That distinction
   * decides whether a food-vetoed want is reachable from here. */
  int cm = (pcity->cm_parameter != NULL) ? 1 : 0;
  int minfood = (pcity->cm_parameter != NULL
                 ? pcity->cm_parameter->minimal_surplus[O_FOOD] : -99);

  if (wonder) {
    bid /= (founder_type == NULL || founder_type->pop_cost > 0) ? 5 : 2;
  }
  if (capped) {
    bid /= 100;
  }
  bid *= (double) trait / TRAIT_DEFAULT_VALUE;

  log_base(deep_metric_log_level(),
           "deep-founderbid turn=%d player=\"%s\" city=%d want=%d"
           " wonder=%d capped=%d maxcities=%d cities=%d trait=%d"
           " food=%d upkeep=%d fed=%d cm=%d minfood=%d"
           " bid=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), pcity->id,
           city_data->founder_want, wonder ? 1 : 0, capped ? 1 : 0,
           adv->max_num_cities, city_list_size(pplayer->cities), trait,
           pcity->surplus[O_FOOD], upkeep, fed ? 1 : 0, cm, minfood, bid);
}

/**********************************************************************//**
  Rescale the two domestic wants ai/default computes for deep, in
  place, for the duration of one domestic_advisor_choose_build() call.

  The caller restores them; they are `ai/default`'s own fields and
  every other consumer must keep seeing the unscaled numbers.

  Both are ints, so a scale rounds. That is the ruleset's own
  granularity for these wants -- daisettlers.c and daicity.c both
  write integers -- and a founder want that rounds to zero is a
  founder the city genuinely will not build at that scale.
**************************************************************************/
static void deep_scale_domestic_bids(struct player *pplayer,
                                     struct city *pcity,
                                     struct ai_city *city_data)
{
  double fs = deep_tuning_get()->scale_settler;
  double ws = deep_tuning_get()->scale_worker;

  if (fs != 1.0 && city_data->founder_want != 0) {
    int was = city_data->founder_want;

    /* The sign is the errand, not the magnitude: negative means "and
     * I need a boat first" (daicity.h:60). Scale the magnitude and
     * put the sign back. */
    city_data->founder_want = (int) (was * fs);
    log_base(deep_metric_log_level(),
             "deep-scale turn=%d player=\"%s\" city=%d who=\"bid\""
             " pick=\"founder\" class=5 scale=%.4f want=%d to=%d",
             game.info.turn, player_name(pplayer), pcity->id, fs,
             was, city_data->founder_want);
  }

  if (ws != 1.0 && city_data->worker_want != 0) {
    int was = city_data->worker_want;

    city_data->worker_want = (int) (was * ws);
    log_base(deep_metric_log_level(),
             "deep-scale turn=%d player=\"%s\" city=%d who=\"bid\""
             " pick=\"worker\" class=6 scale=%.4f want=%d to=%d",
             game.info.turn, player_name(pplayer), pcity->id, ws,
             was, city_data->worker_want);
  }
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
  bool debit_zeroed = FALSE;

  /* The convertible class, censused where it is still visible. Both
   * fields are filled by unit actions during THIS phase and wiped by
   * nullify_prechange_production() at end of turn; deep_manage_cities()
   * runs in last-activities, after every unit has moved, so this is the
   * one vantage that sees them. `carsh` is a Help Wonder ARRIVAL --
   * R2b reported that nothing logs one -- and `disb` is a
   * disband-into-build recovery, the salvage half of the liquidity
   * argument. */
  if (pcity->disbanded_shields > 0 || pcity->caravan_shields > 0) {
    log_base(deep_metric_log_level(),
             "deep-convertible turn=%d player=\"%s\" city=%d"
             " disb=%d carsh=%d stock=%d bcs=%d prod=\"%s\" prodk=%c",
             game.info.turn, player_name(pplayer), pcity->id,
             pcity->disbanded_shields, pcity->caravan_shields,
             pcity->shield_stock, pcity->before_change_shields,
             universal_rule_name(&pcity->production),
             deep_prod_class(&pcity->production));
  }

  deep_bid_log(pplayer, pcity, city_data);
  deep_founder_bid_log(pplayer, pcity, city_data);

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
      /* Rescale the two domestic bids that are not in the auction's
       * currency, HERE, where ai/default has computed them and
       * nothing has consumed them yet. The founder want is a settler
       * count multiplied by the EXPANSIONIST trait and by `expand`;
       * the worker want is a terrain-improvement estimate; the
       * building want they both compete against is a discounted
       * output sum. Scaling the winner instead would leave the
       * advisor's own auction -- the one that decides settler against
       * building -- reading the old currencies. See deepwant.c. */
      int scale_saved_founder = city_data->founder_want;
      int scale_saved_worker = city_data->worker_want;

      deep_scale_domestic_bids(pplayer, pcity, city_data);

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
      city_data->founder_want = scale_saved_founder;
      city_data->worker_want = scale_saved_worker;
      /* Re-price the caravan the domestic advisor just bid for -- or
       * bid for one it did not. Stock's trade-route producer prices a
       * caravan on an income estimate with a HARDCODED distance of ten
       * and no destination, then clamps the want to 0.1 when caravans
       * pile up; deep has picked real destinations since the trade
       * epic and never told the build queue. This is the seam where
       * the two meet: the bid has been formed and nothing has consumed
       * it yet. See deeptrade.c. */
      deep_trade_merit_apply(ait, pplayer, pcity, newchoice);
      /* And the other caravan producer, at the same seam. The wonder
       * caravan is the LARGER claimant on this queue -- R2's census
       * put the winning want at p50 197 for it against 71 for the
       * trade bid -- and it is priced by dai_choose_help_wonder() with
       * the whole wonder's want, undiscounted, divided by the walk.
       * Both switches are independent and both default off. See
       * deeptrade.c. */
      deep_wonder_merit_apply(ait, pplayer, pcity, newchoice);
      /* Price the switch before the auction settles, not after: the
       * cost belongs to the CANDIDATE, and charging the winner after
       * it has won would only ever change whether the winner is built,
       * never which candidate wins. Both bidders are debited by their
       * own forfeit, so a challenger from another production class has
       * to beat the incumbent's class by what leaving it really
       * costs -- and a candidate in the incumbent's class is charged
       * nothing, which is where the hysteresis comes from. */
      {
        bool dom_won_before
          = deep_want_cmp(deep_want_from_choice(newchoice),
                          deep_want_from_choice(&city_data->choice)) > 0;
        adv_want had = (dom_won_before
                        ? newchoice->want : city_data->choice.want);
        bool dom_won_after;

        /* The military classes are rescaled here rather than at bid
         * formation, because deep forms them itself and this is the
         * one place both sides of the comparison exist. The domestic
         * choice is deliberately NOT passed: its settler and worker
         * bids were already scaled before the advisor ran, and its
         * other two outcomes -- a building and a caravan -- are the
         * anchor and R2/R2b's own priced producers. Scale before the
         * switch debit: the debit is an absolute shield-equivalent
         * cost and must be charged against the rescaled bid. */
        deep_want_scale_apply(pplayer, pcity, &(city_data->choice), "mil");

        /* And the bell, on both sides, before the debit: a build that
         * cannot finish is worth its deflator whichever advisor
         * proposed it, and the switch cost is an absolute
         * shield-equivalent that must be charged against the bid the
         * auction will actually compare. */
        deep_endgame_unit_deflate(pplayer, pcity, &(city_data->choice),
                                  "mil");
        deep_endgame_unit_deflate(pplayer, pcity, newchoice, "dom");

        /* And the running cost of keeping what is about to be built,
         * on both sides, before the debit and before the comparison:
         * a unit's lifetime upkeep is the one price in this auction
         * that buildings pay and units did not (audit 1b). */
        deep_unit_upkeep_charge(pplayer, pcity, &(city_data->choice),
                                "mil");
        deep_unit_upkeep_charge(pplayer, pcity, newchoice, "dom");

        deep_switch_cost_apply(pplayer, pcity, &(city_data->choice), "mil");
        deep_switch_cost_apply(pplayer, pcity, newchoice, "dom");

        dom_won_after
          = deep_want_cmp(deep_want_from_choice(newchoice),
                          deep_want_from_choice(&city_data->choice)) > 0;

        if (dom_won_before != dom_won_after) {
          /* The want-winner was rejected on cost. This is the counter
           * the mechanism is judged on: no flips, no hysteresis. */
          log_base(deep_metric_log_level(),
                   "deep-switchflip turn=%d player=\"%s\" city=%d"
                   " won=\"%s\" lost=\"%s\"",
                   game.info.turn, player_name(pplayer), pcity->id,
                   adv_choice_rule_name(dom_won_after
                                        ? newchoice : &(city_data->choice)),
                   adv_choice_rule_name(dom_won_before
                                        ? newchoice : &(city_data->choice)));
        }
        if (dom_won_after) {
          adv_choice_copy(&(city_data->choice), newchoice);
        }
        /* Remembered rather than re-derived below, because a want of
         * zero has three other authors here (the water gate, an empty
         * advisor, a barbarian) and only this one means "the switch was
         * not worth its cost". */
        debit_zeroed = (had > 0 && city_data->choice.want == 0);
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

  /* The hold. A candidate whose want did not survive its own switching
   * cost has not earned a cheaper prize: it has failed to beat what the
   * city is already building, and the honest answer is to go on
   * building it. Handing the city to the fallback ladder here would
   * switch production anyway, on a want of 1, and pay the very forfeit
   * the debit just said was not worth paying.
   *
   * Only when the current production is still legal -- otherwise there
   * is nothing to hold and the ladder is right. */
  if (debit_zeroed
      && city_data->choice.want == 0
      && can_city_build_now(nmap, pcity, &pcity->production,
                            RPT_CERTAIN)) {
    log_base(deep_metric_log_level(),
             "deep-switchhold turn=%d player=\"%s\" city=%d prod=\"%s\""
             " stock=%d",
             game.info.turn, player_name(pplayer), pcity->id,
             universal_rule_name(&pcity->production), pcity->shield_stock);
    return;
  }

  /* Fallbacks */
  if (city_data->choice.want == 0) {
    /* Fallbacks do happen with techlevel 0, which is now default. -- Per */
    CITY_LOG(LOG_DEBUG, pcity, "Falling back - didn't want to build"
             " soldiers, workers, caravans, settlers, or buildings!");
    /* The fallback population, unconditionally: these are the
     * city-turns where every producer bid zero, which is the only
     * population a surplus-shield SINK (Coinage, a shield bank, a
     * low-want build) could be compared on. Logged before the ladder
     * runs, so the surplus is the one the sink would receive. */
    {
      int coinage = 0;

      improvement_iterate(pimprove) {
        if (is_convert_improvement(pimprove)
            && can_city_build_improvement_now(pcity, pimprove,
                                              RPT_CERTAIN)) {
          coinage = 1;
          break;
        }
      } improvement_iterate_end;

      log_base(deep_metric_log_level(),
               "deep-fallback turn=%d player=\"%s\" city=%d"
               " surplus=%d stock=%d size=%d prod=\"%s\" coinage=%d",
               game.info.turn, player_name(pplayer), pcity->id,
               pcity->surplus[O_SHIELD], pcity->shield_stock,
               city_size_get(pcity),
               universal_rule_name(&pcity->production), coinage);
    }
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

    deep_prodswitch_log(ait, pplayer, pcity, &build_new);

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
        .surv = deep_city_survival_verdict(&(wld.map), pcity,
                                           deep_tuning_get()
                                             ->survival_horizon, NULL),
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
             " surv=%.3f urgency=%u capital=%d worth=%d",
             game.info.turn, player_name(pplayer), i,
             emergencies[i].id, emergencies[i].surv,
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
  /* AFTER dai_build_adv_adjust() has zeroed wonder wants outside the
   * wonder city, so the population is exactly that city's candidates. */
  deep_wonder_race(ait, pplayer);
  TIMING_LOG(AIT_BUILDINGS, TIMER_STOP);
  /* AFTER dai_clear_tech_wants(), like every other tech_want writer:
   * the array is wiped at the top of this phase and refilled here. */
  deep_defensive_techwall(ait, pplayer);

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

  deep_spend_gold(ait, pplayer);

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
  deep_fort_census(pplayer);
  deep_worker_travel_census(pplayer);
  deep_trunk_census(pplayer);
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
  /* Immediately after the build choices that accumulate it. */
  deep_birth_census(pplayer);
  /* After taxes and production are settled: if the treasury still
   * cannot cover the coming turn, choose our own sacrifice before
   * the server chooses one for us. */
  deep_balance_treasury(ait, pplayer);
  /* The watermark for rung 3, taken AFTER the treasury guard: gold the
   * guard raised to stay solvent is not spendable income. */
  deep_late_mark = pplayer->economic.gold;
  TIMING_LOG(AIT_TECH, TIMER_START);
  dai_manage_tech(ait, pplayer);
  TIMING_LOG(AIT_TECH, TIMER_STOP);
  dai_manage_spaceship(pplayer);

  /* LAST, and this is the honest end of deep's phase. */
  deep_late_defence_buy(ait, pplayer);
  deep_sell_defend_census(pplayer);

  TIMING_LOG(AIT_ALL, TIMER_STOP);
}
