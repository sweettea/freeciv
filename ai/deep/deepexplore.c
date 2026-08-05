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

/**********************************************************************//**
  How much this player should currently want to build explorers, in
  build-want units: visible unclaimed huts plus unexplored area, with
  diminishing returns per explorer already owned. Zero when the map was
  generated without huts, so hut-free behaviour stays stock (the
  discovery term is deliberately inside the gate for now; widening it
  is a W08-era decision).
**************************************************************************/
adv_want deep_explore_want(struct player *pplayer, int queued)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int huts_visible = 0, unknown = 0, explorers = 0;
  double hut_weight = 0.0;
  bool hut_risk;
  Continent_id ours[64];
  int nours = 0, i;
  adv_want want;

  if (wld.map.server.huts <= 0) {
    return 0;
  }

  /* Hut-risk awareness (task #18; human rule 2026-07-31: exploring
   * away from home, the worst case is a lost explorer -- a fine
   * trade for the tech and gold -- but a hut popped NEXT TO OUR
   * CITIES can spawn the barbarians on our doorstep). Huts within
   * hut_risk_radius of an own city count at hut_risk_discount, and
   * only while the server can actually unleash hut barbarians (the
   * exact gate barbarian.c uses: rate + onset). v3b's tuner priced
   * around this blindly by zeroing hut value on classic; the code
   * now knows why. */
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

      if (hut_risk && tune->hut_risk_radius > 0) {
        city_list_iterate(pplayer->cities, hc) {
          if (real_map_distance(city_tile(hc), ptile)
              <= tune->hut_risk_radius) {
            w = tune->hut_risk_discount;
            break;
          }
        } city_list_iterate_end;
      }
      huts_visible++;
      hut_weight += w;
    }
  } whole_map_iterate_end;

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
    double prize = hut_weight * tune->explore_hut_value
                   + unknown * tune->explore_discovery_value;
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
             " claimed=%.2f saturated=%d",
             game.info.turn, player_name(pplayer), (int) want,
             huts_visible, unknown, explorers, queued, hut_weight,
             prize, claimed, left <= 0.0 ? 1 : 0);
  }

  return want;
}
