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

/* utility */
#include "log.h"

/* common */
#include "city.h"
#include "extras.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server */
#include "maphand.h"

/* ai */
#include "handicaps.h"

#include "deeptuning.h"

#include "deepexplore.h"

/* Exploration want (W06). No advisor in the stock AI ever wants an
 * explorer built: exploration happens only as an AIUNIT_NONE fallback,
 * and dai_spend_gold() disbands explorers that come home. In hut games
 * that forfeits one of the strongest early investments. */

/* What the hut-risk term actually looked at, for the census line.
 * `unleashable` is the population the SERVER would let spawn
 * barbarians (no city of ANY player within the max city map --
 * default.lua:112 and hut_get_limited(), server/unittools.c:3321);
 * `discounted` is the population the code in force actually charged
 * for. The two are separate on purpose: they are not the same set,
 * and the whole point of the census is to show by how much
 * (findings/knob-coverage.md lists both hut_risk_* knobs
 * NO-INSTRUMENT, needing exactly this). */
struct deep_hut_census {
  int discounted;
  int unleashable;
  bool armed;                 /* the server's rate+onset gate is open */
};

/**********************************************************************//**
  The visible prize an explorer fleet exists to collect, as a SUM OF
  PER-PURPOSE TERMS, each with its own gate. Shared by the build side
  and the salvage side, so acquisition and disposal are keyed to the
  same quantity and cannot disagree.

  Deliberately a sum rather than one fused formula.
  findings/non-omniscient-roadmap.md stage 5 puts THREE terms here:

    hut       -- a real object on the map. Live today. No fog gate:
                 popping a hut is worth the same whether or not we
                 could have seen it coming.
    discovery -- unexplored area. Fog-gated and dormant: `unknown` is
                 forced to 0 for a player without H_MAP, which is
                 every difficulty anyone plays (RC6). Returns when
                 W04 makes deep honest.
    patrol    -- NOT YET WRITTEN. An AREA-shaped surveillance term:
                 staleness of territory we already mapped, times the
                 chance something changed there. A rival's quiet
                 fourth city in land we mapped forty turns ago is how
                 an honest AI gets strategically surprised. Also
                 fog-gated, also zero today.

  Adding the third must be adding a term and a gate here, not
  redesigning the shape. The raw components come back through the out
  parameters for the census line; the return value is the summed
  prize, so there is exactly one place where the terms are added.
**************************************************************************/
static double deep_explore_prize(struct player *pplayer, double *hutw_out,
                                 int *unknown_out, int *huts_out,
                                 struct deep_hut_census *census)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int huts_visible = 0, unknown = 0;
  int huts_discounted = 0, huts_unleashable = 0;
  double hut_weight = 0.0;
  bool hut_risk;
  Continent_id ours[64];
  int nours = 0, i;

  /* Hut-risk awareness (task #18), in two forms, one of which is
   * wrong and is the shipped default until an A/B says otherwise.
   *
   * The RADIUS form (hut_risk_radius > 0, honest == 0) implements the
   * human rule of 2026-07-31: a hut popped next to our cities can
   * spawn barbarians on our doorstep, so huts within hut_risk_radius
   * of an OWN city count at hut_risk_discount.
   *
   * The server disagrees, and the server decides.
   * _deflua_hut_get_barbarians (data/default/default.lua:106-129)
   * refuses to unleash ANYTHING when the hut tile satisfies
   * city_exists_within_max_city_map(true) -- any player's city within
   * CITY_MAP_MAX_RADIUS. hut_get_limited (server/unittools.c:3321)
   * carries the identical guard. So a hut near a city is the SAFE
   * one, and the huts that unleash are the ones far from every city:
   * the radius form discounts exactly the risk-free population and
   * pays full value for the dangerous one. The HONEST form
   * (hut_risk_honest > 0) uses the server's predicate and discounts
   * the complementary set. Three mismatches, all fixed by using the
   * server's own test: the sign, our-cities-only vs every player's,
   * and real_map_distance vs the squared-radius city map.
   *
   * The gate is right either way: rate + onset, exactly what
   * unleash_barbarians() tests (server/barbarian.c:283).
   *
   * DEFAULT 0 = the radius form, i.e. today's behaviour byte for
   * byte. The correction is not free to switch on -- it moves an
   * explore-value term on every hut map, and v3b's tuner has been
   * pricing around the wrong-signed term by zeroing hut value on
   * classic, so the interaction between this flip and
   * explore_hut_value is a two-factor question, not a one-line fix.
   * huts_disc / huts_unleash on deep-explore size both populations
   * first. */
  hut_risk = BARBS_DISABLED != game.server.barbarianrate
             && game.info.turn >= game.server.onsetbarbarian;

  /* Continents where this player has feet on the ground: an explorer
   * can only walk its own landmass, so unknown or hutted tiles
   * elsewhere are not prizes it can claim -- the seed-205 games
   * pumped 96 explorers off an unknown-tile count that was mostly
   * ocean and the enemy's island, unreachable and (pre-W04, with
   * decisions running on ground truth anyway) worthless even if
   * reached. */
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
    Continent_id c = tile_continent(ptile);
    bool reachable = FALSE;

    if (c <= 0) {
      continue;   /* Ocean or unassigned: no explorer walks there. */
    }
    for (i = 0; i < nours; i++) {
      if (ours[i] == c) {
        reachable = TRUE;
        break;
      }
    }
    if (!reachable) {
      continue;
    }
    if (!map_is_known(ptile, pplayer)) {
      unknown++;
    } else if (hut_on_tile(ptile)) {
      double w = 1.0;
      bool unleashable;

      /* The server's own predicate, asked of every hut whether or not
       * the term in force reads it: a hut with no city of ANY player
       * within CITY_MAP_MAX_RADIUS_SQ is one the server would let
       * unleash a horde, and every other hut is one it refuses to.
       * Counted before any gate so that "we never looked" and "we
       * looked and there were none" stay distinguishable -- the
       * lesson deep-fort's offers/evals split recorded. */
      unleashable = !city_exists_within_max_city_map(nmap, ptile, TRUE);
      if (unleashable) {
        huts_unleashable++;
      }

      if (hut_risk && tune->hut_risk_honest > 0) {
        /* The honest form: discount the huts the server would let
         * unleash, which are the ones NO city stands near. No radius
         * knob participates -- the radius is the server's own
         * CITY_MAP_MAX_RADIUS_SQ and is not ours to move. */
        if (unleashable) {
          w = tune->hut_risk_discount;
          huts_discounted++;
        }
      } else if (hut_risk && tune->hut_risk_radius > 0) {
        city_list_iterate(pplayer->cities, hc) {
          if (real_map_distance(city_tile(hc), ptile)
              <= tune->hut_risk_radius) {
            w = tune->hut_risk_discount;
            huts_discounted++;
            break;
          }
        } city_list_iterate_end;
      }
      huts_visible++;
      hut_weight += w;
    }
  } whole_map_iterate_end;

  /* Discovery pays only for a player that cannot already see through
   * the fog. At every level a human actually plays, H_MAP is unset
   * (ai/difficulty.c:127 -- hard has "no actual handicaps"), so deep's
   * own settler routing runs parameter.omniscience = TRUE
   * (deepsettler.c:258,305,412,456,535) and its site evaluator scores
   * terrain the explorer would be "revealing". Paying to uncover a map
   * the planner already reads is RC6 billing itself twice.
   *
   * Europe T94 measured the cost exactly: want=31 from unknown=6461
   * with huts_vis=0 -- an entire explorer programme, in four cities at
   * once, buying information deep was already acting on. This does not
   * touch the hut term, which is a real prize either way, and the
   * discovery term returns in full the moment W04 makes deep honest. */
  if (!has_handicap(pplayer, H_MAP)) {
    unknown = 0;
  }

  *hutw_out = hut_weight;
  *unknown_out = unknown;
  *huts_out = huts_visible;
  census->discounted = huts_discounted;
  census->unleashable = huts_unleashable;
  census->armed = hut_risk;

  /* The sum. One line per purpose; a fourth purpose is a fourth line. */
  return hut_weight * tune->explore_hut_value          /* huts */
         + unknown * tune->explore_discovery_value;    /* discovery */
         /* + patrol: roadmap stage 5, fog-gated, not written */
}

/**********************************************************************//**
  How much this player should currently want to build explorers, in
  build-want units. Zero when the map was generated without huts, so
  hut-free behaviour stays stock.
**************************************************************************/
adv_want deep_explore_want(struct player *pplayer, int queued)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int huts_visible = 0, unknown = 0, explorers = 0;
  double hut_weight = 0.0, prize;
  struct deep_hut_census hutc = { 0, 0, FALSE };
  adv_want want;

  if (wld.map.server.huts <= 0) {
    return 0;
  }

  prize = deep_explore_prize(pplayer, &hut_weight, &unknown, &huts_visible,
                             &hutc);

  unit_list_iterate(pplayer->units, punit) {
    if (unit_has_type_role(punit, L_EXPLORER)) {
      explorers++;
    }
  } unit_list_iterate_end;
  /* Explorers already ON THE WAY count too. `queued` only covers claims
   * made in the SAME management pass, and a unit under construction is
   * not yet in pplayer->units, so a city building one was invisible to
   * both -- and a city with no shield surplus stays invisible for
   * dozens of turns while the want re-orders around it. Europe T94:
   * four Carthaginian cities on Explorer at once, three of them with
   * shield surplus 0. */
  city_list_iterate(pplayer->cities, pcity) {
    if (VUT_UTYPE == pcity->production.kind
        && utype_has_role(pcity->production.value.utype, L_EXPLORER)) {
      explorers++;
    }
  } city_list_iterate_end;

  /* queued = explorer builds already claimed by other cities in this
   * same management pass, so one global want cannot order a fleet
   * (25 cities x one number was the other half of the pump).
   *
   * The MARGINAL explorer, not the average one. Dividing the whole
   * visible prize by the fleet is a hyperbola: it approaches zero and
   * never arrives, so a city whose own best build want is 0 keeps
   * taking explorers at a printed want of 0 for the rest of the game,
   * and nothing ever disbands them. Measured on isolated-150 before
   * this change: standing fleets of 51, 37 and 21 explorers with two
   * to six huts in sight, want printing 0 from T120 while claims
   * continued to T150 -- an equilibrium, not a transient, and it
   * survived the reachability filter above because that filter is
   * already in force.
   *
   * So price what is LEFT: each explorer already fielded or on order
   * is expected to work explore_explorer_claim huts' worth of the
   * prize, and only the uncovered remainder creates demand. The want
   * then reaches exactly zero once the fleet covers the prizes, which
   * is the property the hyperbola lacked, and the fleet's ceiling is
   * a ratio of two measured quantities rather than a cap. The
   * discovery term rides the same subtraction on purpose: when W04
   * makes `unknown` live again it must not pump under fog either.
   *
   * Claim is denominated in hut-equivalents rather than raw want, so
   * tuning explore_hut_value rescales the prize and the claim
   * together and the saturation point stays where it was put. */
  {
    double claimed = (explorers + queued)
                     * tune->explore_explorer_claim
                     * tune->explore_hut_value;
    double left = prize - claimed;

    if (left < 0.0) {
      left = 0.0;
    }
    want = left / (1 + explorers + queued);

    log_base(deep_metric_log_level(),
             "deep-explore turn=%d player=\"%s\" want=%d huts_vis=%d"
             " unknown=%d explorers=%d queued=%d hutw=%.2f prize=%.2f"
             " claimed=%.2f saturated=%d huts_disc=%d huts_unleash=%d"
             " barbs_armed=%d",
             game.info.turn, player_name(pplayer), (int) want,
             huts_visible, unknown, explorers, queued, hut_weight,
             prize, claimed, left <= 0.0 ? 1 : 0,
             hutc.discounted, hutc.unleashable, hutc.armed ? 1 : 0);
  }

  return want;
}

/**********************************************************************//**
  Explorers this player owns beyond what the visible prize can employ:
  the DISPOSAL side of the same marginal quantity the build side reads.

  The old disband hack (deepdefence.c) was gated on the explore want
  being zero, and the want was a hyperbola that never reached zero, so
  in a hut game it never fired -- which is exactly how a fleet of 51
  became an equilibrium. Now that the want has a real zero, the loop
  can close.

  Three properties the bare hack lacked:

  - HYSTERESIS. The build point is "the prize can employ another
    explorer"; the salvage point is that plus explore_salvage_gap
    explorers of slack. Without the gap, build and disband alternate
    on the same turn's arithmetic and the empire churns shields.
  - A RETENTION FLOOR. explore_retain_floor explorers are never
    salvaged: island hut supply reopens as new landmasses are reached
    (the seed-205 observation that opened this item), and the two
    dormant terms of deep_explore_prize() land on this same fleet the
    day W04 makes them live -- the floor keeps a scout alive for the
    day re-scouting matters (roadmap stage 5).
  - It is keyed to the same prize, so a hut appearing rebuilds and a
    hut consumed retires, on one number.

  Returns how many may be salvaged now, never negative.
**************************************************************************/
int deep_explore_surplus(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int huts_visible = 0, unknown = 0, explorers = 0;
  double hut_weight = 0.0, employable;
  struct deep_hut_census hutc = { 0, 0, FALSE };
  int surplus;

  if (wld.map.server.huts <= 0
      || tune->explore_explorer_claim <= 0.0
      || tune->explore_hut_value <= 0.0) {
    return 0;   /* No prize, or the saturation shape is switched off. */
  }

  unit_list_iterate(pplayer->units, punit) {
    if (unit_has_type_role(punit, L_EXPLORER)) {
      explorers++;
    }
  } unit_list_iterate_end;
  if (explorers <= tune->explore_retain_floor) {
    return 0;
  }

  /* Explorers the whole prize can employ, in the build side's own
   * units: the SAME summed prize, divided by what one explorer is
   * deemed to collect. A dormant term waking up raises this and the
   * salvage stops, with no second formula to keep in step. */
  employable = deep_explore_prize(pplayer, &hut_weight, &unknown,
                                  &huts_visible, &hutc)
               / (tune->explore_explorer_claim * tune->explore_hut_value);

  surplus = explorers - (int) (employable + tune->explore_salvage_gap);
  if (surplus > explorers - tune->explore_retain_floor) {
    surplus = explorers - tune->explore_retain_floor;
  }

  return MAX(surplus, 0);
}
