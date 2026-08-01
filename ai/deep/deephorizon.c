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
#include "effects.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "player.h"
#include "requirements.h"
#include "research.h"
#include "specialist.h"
#include "tech.h"
#include "tile.h"

/* server/advisors */
#include "advdata.h"
#include "advtools.h"
#include "infracache.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daiplayer.h"

#include "deepcombat.h"
#include "deeptuning.h"

#include "deephorizon.h"

/* Horizon-aware improvement value (W03). Stock base_want() measures an
 * improvement's delta at today's city size, so buildings whose entire
 * value is future growth (aqueduct, sewer, granary, harbour) score ~0
 * and only multiplier buildings get built on merit; and effects that
 * need BOTH a building and a tile extra (civ2civ3 farmland +
 * supermarket) deadlock at zero from either side. Deep adjusts the
 * stock building_want[] array after the building advisor has spoken:
 * capacity-gating buildings get credit for the growth they unblock,
 * and bundle buildings get credit for the joint effect value over the
 * catchment tiles that could carry the partner extra. */

/**********************************************************************//**
  Mean weighted-output value of the best currently-unworked workable
  tiles of the city -- what a new citizen would earn, in the same
  currency as the tile sums (W02).
**************************************************************************/
static adv_want deep_unworked_tile_value(struct city *pcity,
                                         struct adv_data *adv, int count)
{
  const struct civ_map *nmap = &(wld.map);
  int best[3] = {0, 0, 0};
  int n = 0, i;
  adv_want total = 0;

  fc_assert_ret_val(count <= 3, 0);

  city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                    city_tile(pcity), ptile) {
    int sum;

    if (tile_worked(ptile) != NULL || ptile == city_tile(pcity)
        || !city_can_work_tile(pcity, ptile)) {
      continue;
    }
    sum = city_tile_output(pcity, ptile, FALSE, O_FOOD)
          * adv->food_priority
          + city_tile_output(pcity, ptile, FALSE, O_TRADE)
            * adv->science_priority
          + city_tile_output(pcity, ptile, FALSE, O_SHIELD)
            * adv->shield_priority;
    for (i = 0; i < 3; i++) {
      if (sum > best[i]) {
        int t = best[i];

        best[i] = sum;
        sum = t;
      }
    }
    n++;
  } city_tile_iterate_end;

  count = MIN(count, MIN(n, 3));
  if (count <= 0) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    total += best[i];
  }
  return total / count;
}

/**********************************************************************//**
  Credit for a building that lifts this city's growth ceiling: the
  blocked growth curve over the planning horizon (H = MORT, the
  documented amortize horizon), valued at what new citizens would earn
  from the best unworked tiles, amortized by time-to-grow.
**************************************************************************/
static adv_want deep_capacity_credit(struct city *pcity,
                                     struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int ttg = city_turns_to_grow(pcity);
  int steps;
  adv_want per_citizen;

  if (city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
    /* Not currently capped; the cap-lifter earns only anticipation. */
    return 0;
  }
  if (ttg <= 0) {
    /* Not growing at all (no food surplus): the cap is not the
     * binding constraint. */
    return 0;
  }

  steps = MAX(1, MORT / MAX(ttg, 1));
  per_citizen = deep_unworked_tile_value(pcity, adv, 3);

  return amortize(per_citizen * steps * tune->horizon_capacity_weight,
                  ttg);
}

/**********************************************************************//**
  Citizens this city is currently spending on keeping order rather than
  on working tiles: entertainer-equivalent specialists (those whose
  luxury output pays the ruleset's happy_cost) plus the citizens still
  unhappy after everything. This is stock's own quantity
  (dai_content_effect_value in daieffects.c), recomputed here because
  its helper is static; keeping the definitions identical means the
  horizon credit EXTENDS stock's WAG rather than competing with it.
**************************************************************************/
static int deep_pacification_load(const struct city *pcity)
{
  int load = pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL];

  normal_specialist_type_iterate(sp) {
    if (get_specialist_output(pcity, sp, O_LUXURY) >= game.info.happy_cost) {
      load += pcity->specialists[sp];
    }
  } normal_specialist_type_iterate_end;

  return load;
}

/**********************************************************************//**
  Credit for a building that makes citizens content: the citizens it
  frees from pacification duty over the planning horizon, valued at
  what a freed citizen would earn from the best unworked tiles -- the
  same currency, and the same MORT horizon, as the capacity credit.

  Unhappiness is the city's SOFT size cap. The hard cap (Size_Adj) stops
  growth outright and deep_capacity_credit() prices it; past the soft
  cap the city keeps growing but each new citizen must entertain instead
  of work, so the marginal citizen earns zero. Both are "the city cannot
  use another worker"; only one of them was priced.

  Stock cannot price this at all. advdata.c's impr_calc switch has the
  whole happiness family #if 0'd out under a TODO, so a building whose
  only effects are Make_Content -- Temple, Colosseum -- gets
  ADV_IMPR_ESTIMATE, and base_want() returns zero for it on its first
  line. Its entire want is 35 per citizen that is unhappy RIGHT NOW,
  which is zero in precisely the city that is about to need it.
**************************************************************************/
static adv_want deep_content_credit(struct city *pcity,
                                    const struct impr_type *pimprove,
                                    struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int amount, load, ttg, arriving, freed;
  adv_want per_citizen;

  if (get_city_bonus(pcity, EFT_NO_UNHAPPY) > 0) {
    /* Nothing to pacify, now or ever. */
    return 0;
  }

  amount = get_potential_improvement_bonus(pimprove, pcity,
                                           EFT_MAKE_CONTENT,
                                           RPT_CERTAIN, FALSE)
           + get_potential_improvement_bonus(pimprove, pcity,
                                             EFT_FORCE_CONTENT,
                                             RPT_CERTAIN, FALSE);
  if (amount <= 0) {
    return 0;
  }

  load = deep_pacification_load(pcity);
  ttg = city_turns_to_grow(pcity);

  /* Citizens due to arrive within the horizon. Past the content limit
   * each of them lands on pacification duty, so they are demand for
   * this building just as surely as today's entertainers are. */
  arriving = (ttg > 0 ? MORT / ttg : 0);

  /* A building can only ever convert its own `amount`. */
  freed = MIN(amount, load + arriving);
  if (freed <= 0) {
    return 0;
  }

  per_citizen = deep_unworked_tile_value(pcity, adv, 3);

  /* Immediate if the load is already there; otherwise it starts paying
   * when the city next grows. */
  return amortize(freed * per_citizen * tune->horizon_content_weight,
                  load > 0 ? 0 : ttg);
}

/**********************************************************************//**
  Credit bundle buildings: effects whose requirements contain BOTH a
  building at city range and an extra at tile range (the civ2civ3
  farmland + supermarket pattern, detected generically). Returns the
  credit for pimprove in pcity, or 0.
**************************************************************************/
static adv_want deep_bundle_credit(struct city *pcity,
                                   const struct impr_type *pimprove,
                                   struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  adv_want credit = 0;

  for (int etype = 0; etype < EFT_COUNT; etype++) {
    effect_list_iterate(get_effects(etype), peffect) {
      bool has_this_building = FALSE;
      const struct extra_type *partner = NULL;

      if (peffect->value <= 0) {
        continue;
      }
      requirement_vector_iterate(&peffect->reqs, preq) {
        if (preq->source.kind == VUT_IMPROVEMENT
            && preq->range == REQ_RANGE_CITY
            && preq->present
            && preq->source.value.building == pimprove) {
          has_this_building = TRUE;
        } else if (preq->source.kind == VUT_EXTRA
                   && preq->range == REQ_RANGE_TILE
                   && preq->present) {
          partner = preq->source.value.extra;
        }
      } requirement_vector_iterate_end;

      if (has_this_building && partner != NULL) {
        int viable = 0;

        city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                          city_tile(pcity), ptile) {
          if (ptile != city_tile(pcity)
              && (tile_has_extra(ptile, partner)
                  || !tile_has_conflicting_extra(ptile, partner))) {
            viable++;
          }
        } city_tile_iterate_end;

        credit += peffect->value * viable * tune->horizon_bundle_weight;
      }
    } effect_list_iterate_end;
  }

  return credit;
}

/**********************************************************************//**
  Post-adjust the stock building_want[] array with the horizon and
  bundle credits, once per city per management pass.
**************************************************************************/
void deep_adjust_building_wants(struct ai_type *ait,
                                struct player *pplayer)
{
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  const struct deep_tuning *tune = deep_tuning_get();

  if (adv == NULL) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    adv_want cap_credit;
    bool threatened;

    /* "Threatened" means the city would probably FALL, not that an
     * enemy is nearby. Stock's urgency and grave_danger are presence
     * counts -- grave_danger increments for any capture-capable unit
     * within one turn's move, urgency within three, neither asking
     * whether the garrison wins -- so a city with five Musketeers
     * behind walls scores the same as an empty one (Europe T106: 69%
     * of deep cities carried urgency, 86% of stock's). Deep owns a
     * probability for exactly this question; use it, and fall back to
     * the stock flags only when the survival model is switched off. */
    if (tune->survival_target > 0.0) {
      int wave = 0;
      double surv = deep_city_stack_survival(&(wld.map), pcity,
                                             tune->survival_horizon,
                                             &wave);

      threatened = wave > 0 && surv < tune->survival_target;
    } else {
      threatened = def_ai_city_data(pcity, ait)->urgency > 0
                   || def_ai_city_data(pcity, ait)->grave_danger > 0;
    }
    cap_credit = deep_capacity_credit(pcity, adv);
    if (threatened && cap_credit <= 0) {
      /* Growth is a peace-time investment: no horizon credits while
       * the city is under threat (the contested ablation showed the
       * credits, not the site terms, carried the war-map score cost:
       * site-only 0.997 vs full stack 0.948).
       *
       * EXCEPT the cap-lifter. A growth CEILING is structural, not a
       * luxury: a city at the ruleset's size gate stays there for the
       * rest of the game, and in a permanent-war world the gate never
       * reopens. Europe T106 measured the cost -- 69% of deep cities
       * carried urgency>0, including 17 of the 19 sitting exactly at
       * the cap, so deep held ZERO Aqueducts across 85 cities and no
       * city exceeded size 8, while stock (86% urgency, no such gate)
       * built 9 Aqueducts and ran cities to 22. This is RC1's own
       * headline example, reintroduced by the gate that was meant to
       * protect a contested-cell score. */
      continue;
    }

    improvement_iterate(pimprove) {
      Impr_type_id id = improvement_number(pimprove);
      adv_want add = 0;

      if (!can_city_build_improvement_now(pcity, pimprove, RPT_CERTAIN)) {
        /* The credit cannot buy a tech-gated building, but it CAN buy
         * the research: a capped city lacking the cap-lifter's tech
         * previously contributed zero pressure toward exactly the
         * advance that would solve its problem (the horizon stopped
         * at the tech wall; findings 2026-07-29). Route the capacity
         * credit into the gating advance's tech want instead. */
        if (cap_credit > 0
            && (building_has_effect(pimprove, EFT_SIZE_ADJ)
                || building_has_effect(pimprove, EFT_SIZE_UNLIMIT))
            && can_city_build_improvement_later(pcity, pimprove)) {
          const struct research *presearch = research_get(pplayer);

          requirement_vector_iterate(&pimprove->reqs, preq) {
            if (VUT_ADVANCE == preq->source.kind && preq->present
                && research_invention_state(presearch,
                     advance_number(preq->source.value.advance))
                   != TECH_KNOWN) {
              def_ai_player_data(pplayer, ait)->tech_want[
                  advance_index(preq->source.value.advance)]
                += cap_credit
                   * deep_tuning_get()->horizon_tech_weight;
              log_base(LOG_DEBUG,
                       "deep-techwall city=%d impr=\"%s\" tech=\"%s\""
                       " credit=%.1f", pcity->id,
                       improvement_rule_name(pimprove),
                       advance_rule_name(preq->source.value.advance),
                       (double) cap_credit);
            }
          } requirement_vector_iterate_end;
        }
        continue;
      }

      if (cap_credit > 0
          && (building_has_effect(pimprove, EFT_SIZE_ADJ)
              || building_has_effect(pimprove, EFT_SIZE_UNLIMIT))) {
        add += cap_credit;
      }
      if (cap_credit > 0
          && building_has_effect(pimprove, EFT_GROWTH_FOOD)) {
        /* Growth accelerator: same currency, shaped separately. */
        add += cap_credit * deep_tuning_get()->horizon_growth_weight;
      }
      if (!threatened) {
        add += deep_bundle_credit(pcity, pimprove, adv);
        add += deep_content_credit(pcity, pimprove, adv);
      }

      if (!threatened && game.info.illness_on
          && city_size_get(pcity) + 1 >= game.info.illness_min_size) {
        int health = get_potential_improvement_bonus(pimprove, pcity,
                                                     EFT_HEALTH_PCT,
                                                     RPT_CERTAIN, FALSE);

        if (health > 0) {
          /* Plague insurance is worth having BEFORE the size gate the
           * building's other effects wait for (civ2civ3: Aqueduct
           * cuts illness 30% from size 5; Size_Adj matters at 8). */
          add += health * deep_tuning_get()->horizon_health_weight;
        }
      }

      if (add > 0) {
        /* The credit imagines the building's future yields, but the
         * ruleset may charge gold every turn to keep it (civ2civ3:
         * every building; head-to-head games showed deep bankrupting
         * itself on exactly that bill, gold ratio 0.13-0.55 vs
         * stock). Charge the ruleset's own upkeep figure against the
         * credit -- never below zero, so stock's base want keeps its
         * pricing. */
        add -= city_improvement_upkeep(pcity, pimprove)
               * deep_tuning_get()->upkeep_gold_weight;
        add = MAX(add, 0);
      }
      if (add > 0) {
        pcity->server.adv->building_want[id] += add;
        log_base(LOG_DEBUG,
                 "deep-horizon city=%d impr=\"%s\" credit=%.1f",
                 pcity->id, improvement_rule_name(pimprove),
                 (double) add);
      }
    } improvement_iterate_end;
  } city_list_iterate_end;
}
