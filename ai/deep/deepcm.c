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
#include "mem.h"
#include "timing.h"

/* common */
#include "city.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"
#include "fc_types.h"
#include "game.h"
#include "improvement.h"
#include "player.h"

/* common/aicore */
#include "cm.h"

/* server */
#include "cityturn.h"

/* server/advisors */
#include "advgoto.h"

/* ai/default */
#include "daicity.h"
#include "daimilitary.h"
#include "daidata.h"
#include "daiplayer.h"

#include "deepcombat.h"
#include "deeptuning.h"

#include "deepcm.h"

/* City manager parameters from city state (W09-lite). Stock AI cities
 * all run the hardcoded factors in set_default_city_manager()
 * (cityturn.c) -- the comment there says priorities should someday
 * flow in via a separate set of variables, and pcity->cm_parameter is
 * exactly that hook: auto_arrange_workers() honours it when set, and
 * nothing in the stock AI ever sets it. Cities that match no rule get
 * their cm_parameter cleared, so the change is incremental and
 * reversible per city. */

/* The postures, in the priority order the state machine walks them:
 * war beats everything, then the growth cap, then a wonder in
 * progress, then grow-hard. A city that matches none runs the stock
 * default. Named as an enum rather than left implicit in an if-chain
 * because the chain now has to be evaluated more than once for the
 * same city -- once for the posture the city takes, and once for the
 * posture it would have taken instead. */
enum deep_cm_posture {
  DEEP_CM_NO_POSTURE = 0,
  DEEP_CM_WAR,
  DEEP_CM_CAP,
  DEEP_CM_WONDER,
  DEEP_CM_GROW
};

/**********************************************************************//**
  Fill the stock default booleans and minimal surpluses; only the
  factors differ between deep's rules (keeping the safety rails while
  the factors go through W25 tuning).
**************************************************************************/
static void deep_cm_base(struct cm_parameter *cmp, struct city *pcity)
{
  int csize = city_size_get(pcity);
  int gsize = city_granary_size(csize);

  cm_init_parameter(cmp);
  cmp->require_happy = FALSE;
  cmp->allow_disorder = FALSE;
  cmp->allow_specialists = TRUE;

  if (gsize == pcity->food_stock) {
    cmp->minimal_surplus[O_FOOD] = 0;
  } else {
    cmp->minimal_surplus[O_FOOD] = 1;
  }
  cmp->minimal_surplus[O_SHIELD] = 1;
  cmp->minimal_surplus[O_TRADE] = 0;
  cmp->minimal_surplus[O_GOLD] = -FC_INFINITY;
  cmp->minimal_surplus[O_LUXURY] = 0;
  cmp->minimal_surplus[O_SCIENCE] = 0;

  /* Stock values for the outputs deep's rules leave alone. */
  cmp->factor[O_GOLD] = 2;
  cmp->factor[O_LUXURY] = 0;
  cmp->factor[O_SCIENCE] = 2;
}

/**********************************************************************//**
  How trade-rich this city's catchment is relative to its shields:
  (1 + total workable trade) / (1 + total workable shields). The right
  trade weight is a function of what the map offers -- a constant
  tuned on poor land starves specials-rich cities (the isolated -405
  lesson, STATUS 2026-07-30: G cut cm_cap_trade for cramped and
  bankrupted isolated; H kept it high). Raised to the tunable slope
  this scales every state's trade factor: slope 0 restores the plain
  constant, slope 1 is fully proportional.
**************************************************************************/
static double deep_cm_trade_richness(struct city *pcity)
{
  const struct civ_map *nmap = &(wld.map);
  int trade = 0, shield = 0;

  city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                    city_tile(pcity), ptile) {
    if (ptile == city_tile(pcity)) {
      continue;
    }
    trade += city_tile_output(pcity, ptile, FALSE, O_TRADE);
    shield += city_tile_output(pcity, ptile, FALSE, O_SHIELD);
  } city_tile_iterate_end;

  return (1.0 + trade) / (1.0 + shield);
}

/**********************************************************************//**
  Attack ratings of the city's garrison, weighted by the sortie knob:
  with killstack, an attack-heavy unit in the keep defends by erasing
  the besieging stack, value the fortified-defence sums never see.
**************************************************************************/
static double deep_cm_sortie_value(struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double sortie = 0.0;

  if (tune->sortie_defence_weight <= 0.0) {
    return 0.0;
  }
  unit_list_iterate(city_tile(pcity)->units, punit) {
    if (unit_owner(punit) == city_owner(pcity)
        && !unit_has_type_flag(punit, UTYF_CIVILIAN)) {
      sortie += adv_unit_att_rating(punit);
    }
  } unit_list_iterate_end;
  return sortie * tune->sortie_defence_weight;
}

/**********************************************************************//**
  Sequential-attrition check on the grow-release: the linear margin
  test below passes at parity, but parity against a wave of N means
  winning N straight duels (the exact overreach the margin knob's own
  comment admits to papering over). With a wave observed, grow-hard
  resumes only when the garrison's survival odds also clear the
  target; with no observed wave the linear test keeps sole custody,
  so synthetic-fear-only cities (boat shadow, density) behave exactly
  as before.
**************************************************************************/
static bool deep_cm_survival_ok(struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int wave = 0;
  double surv;

  if (tune->survival_target <= 0.0) {
    return TRUE;
  }
  surv = deep_city_survival_verdict(&(wld.map), pcity,
                                    tune->survival_horizon, &wave);
  return wave == 0 || surv >= tune->survival_target;
}

/**********************************************************************//**
  The posture a city takes when it is NOT on war footing: the second,
  third and fourth arms of the state machine, in their original order.
  Split out from the chain so that the war branch's counterfactual has
  something to compare against -- "what would this city be doing if it
  were not at war" is exactly this function.
**************************************************************************/
static enum deep_cm_posture deep_cm_peace_posture(struct ai_type *ait,
                                                  struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct ai_city *city_data = def_ai_city_data(pcity, ait);

  if (!city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
    /* Growth-capped: food is nearly worthless, work the mines and
     * trade specials instead. */
    return DEEP_CM_CAP;
  }
  if (VUT_IMPROVEMENT == pcity->production.kind
      && is_wonder(pcity->production.value.building)) {
    /* Wonder under construction: shields, with trade kept up to
     * fund completion. */
    return DEEP_CM_WONDER;
  }
  if (city_size_get(pcity) < tune->cm_grow_below
      && (city_data->danger == 0
          || (assess_defense(ait, pcity)
              + deep_cm_sortie_value(pcity)
              >= tune->cm_grow_safety_margin * city_data->danger
              && deep_cm_survival_ok(pcity)))) {
    /* Young city with headroom whose fear, if any, is already PAID
     * FOR: grow-hard yields to the garrison only while defence
     * lags the assessed danger. assess_defense_quadratic()
     * returns (sum of linear defense)^2, so this is plain
     * linear defense >= linear danger, written in the squared
     * currency the build side's danger-percent formula uses --
     * the two sides agree bit-for-bit on when a garrison is
     * "enough" -- linear defence plus sortie value vs margined
     * danger. The margin multiplies danger before comparison
     * and deliberately spans BOTH directions of the argument:
     * below 1, because defence multipliers (walls, fortify,
     * terrain) make small garrisons genuinely repel big attack
     * sums and assess_danger() overcounts (it sums every unit in
     * radius; they rarely all come); above 1, because attackers
     * strike sequentially and parity means winning N straight
     * fights. Ships neutral at 1.0; the tuner referees. Growth
     * never comes at the expense of needed defenders, but a
     * satisfied garrison never taxes growth (human refinement
     * 2026-07-30; the garden2b pile regression was grow-mad
     * shieldless coasts).
     * Deep runs fewer cities than stock at stock's growth pace
     * (mean size 3.5->5.4 IDENTICAL both sides, garden2b); quality
     * must compensate quantity, and civ2civ3 has no rapture, so
     * the CM food weight is the only growth lever there is. */
    return DEEP_CM_GROW;
  }
  return DEEP_CM_NO_POSTURE;
}

/**********************************************************************//**
  Write a posture's factors into a parameter already filled by
  deep_cm_base(). Returns FALSE for the no-posture case, which means
  the city runs the stock default and gets its cm_parameter cleared.
**************************************************************************/
static bool deep_cm_apply_posture(struct cm_parameter *cmp,
                                  struct city *pcity,
                                  enum deep_cm_posture posture)
{
  const struct deep_tuning *tune = deep_tuning_get();

  switch (posture) {
  case DEEP_CM_WAR:
    /* Under threat: shields now; the happy factor stops the
     * arrangement fighting the (stock-ignored) happiness signal. */
    cmp->factor[O_FOOD] = tune->cm_war_food;
    cmp->factor[O_SHIELD] = tune->cm_war_shield;
    cmp->factor[O_TRADE] = tune->cm_war_trade;
    break;
  case DEEP_CM_CAP:
    cmp->factor[O_FOOD] = tune->cm_cap_food;
    cmp->factor[O_SHIELD] = tune->cm_cap_shield;
    cmp->factor[O_TRADE] = tune->cm_cap_trade;
    break;
  case DEEP_CM_WONDER:
    cmp->factor[O_FOOD] = tune->cm_war_food;
    cmp->factor[O_SHIELD] = tune->cm_wonder_shield;
    cmp->factor[O_TRADE] = tune->cm_wonder_trade;
    break;
  case DEEP_CM_GROW:
    cmp->factor[O_FOOD] = tune->cm_grow_food;
    cmp->factor[O_SHIELD] = tune->cm_grow_shield;
    cmp->factor[O_TRADE] = tune->cm_grow_trade;
    break;
  case DEEP_CM_NO_POSTURE:
    return FALSE;
  }
  cmp->happy_factor = tune->cm_happy_factor;

  if (tune->cm_trade_quality_slope > 0.0) {
    /* Let the map set the trade appetite: scale the state's trade
     * factor by catchment trade-richness. */
    double scaled = cmp->factor[O_TRADE]
                    * pow(deep_cm_trade_richness(pcity),
                          tune->cm_trade_quality_slope);

    cmp->factor[O_TRADE] = MAX(0, (int) (scaled + 0.5));
  }
  return TRUE;
}

/* One remembered posture counterfactual per city per turn.
 *
 * Keyed on the TURN and not cleared from the phase hook, because the
 * question this answers ("what do these two arrangements yield") is a
 * property of the map and the city, and every consumer of it runs
 * inside the same turn. A new game in the same server process starts
 * at a turn number below the one the last game ended on, so the
 * turn stamp also fences games apart. */
struct deep_cm_memo {
  int city_id;
  bool valid;                   /* Both solves found an arrangement */
  enum deep_cm_posture alt;     /* What the city would do instead */
  int war[O_LAST];              /* Surplus on war footing */
  int peace[O_LAST];            /* Surplus under alt */
  double cost;                  /* Growth forgone by the footing, [0,1] */
};

static struct deep_cm_memo *deep_cm_cache = NULL;
static int deep_cm_n = 0;
static int deep_cm_cap = 0;
static int deep_cm_turn = -1;

/* The solver-time bill, reported per player-turn so the counterfactual
 * pays for itself in public. Counted for the WHOLE turn (all deep
 * players), reset with the cache. */
static int deep_cm_solves = 0;
static double deep_cm_seconds = 0.0;

/**********************************************************************//**
  The surplus DELTA between the posture this city took and the one it
  would have taken instead: war minus alternative, per output.

  Returns FALSE when no counterfactual was taken for this city this
  turn, which is the common case: the pair of solves is run only for
  cities the war gate was about to flip, because that is the only
  decision that currently pays for it. A reader outside deepcm.c
  (the fear machinery's satisfied-city discount is the intended one)
  must therefore treat FALSE as "no opinion" and not as "costs
  nothing".
**************************************************************************/
bool deep_cm_posture_delta(const struct city *pcity, int *food,
                           int *shield, int *trade)
{
  int i;

  if (deep_cm_turn != game.info.turn) {
    return FALSE;
  }
  for (i = 0; i < deep_cm_n; i++) {
    const struct deep_cm_memo *e = &deep_cm_cache[i];

    if (e->city_id == pcity->id) {
      if (!e->valid) {
        return FALSE;
      }
      if (food != NULL) {
        *food = e->war[O_FOOD] - e->peace[O_FOOD];
      }
      if (shield != NULL) {
        *shield = e->war[O_SHIELD] - e->peace[O_SHIELD];
      }
      if (trade != NULL) {
        *trade = e->war[O_TRADE] - e->peace[O_TRADE];
      }
      return TRUE;
    }
  }
  return FALSE;
}

/**********************************************************************//**
  Run the city manager under BOTH candidate postures and remember what
  each yields.

  A posture change has never been priced anywhere in this codebase: the
  decision to put a city on war footing swaps one factor set for
  another and the food and growth it gives up appear in no want, no
  danger and no counter. The solver that answers the question is
  already here -- cm_query_result() does not touch the city -- so the
  honest price is one extra pair of evaluates.

  The alternative posture is deep_cm_peace_posture(), or stock's own
  default when that returns none: a mature city outside grow-hard has
  no deep posture at all, and comparing war footing against deep's
  BASE parameter rather than against what the city would really run
  would price the change against an arrangement that never happens.
**************************************************************************/
static const struct deep_cm_memo *
deep_cm_counterfactual(struct ai_type *ait, struct city *pcity)
{
  struct deep_cm_memo *e;
  struct cm_parameter war, peace;
  struct cm_result *res;
  struct timer *clock;
  int i;

  if (deep_cm_turn != game.info.turn) {
    deep_cm_turn = game.info.turn;
    deep_cm_n = 0;
    deep_cm_solves = 0;
    deep_cm_seconds = 0.0;
  }
  for (i = 0; i < deep_cm_n; i++) {
    if (deep_cm_cache[i].city_id == pcity->id) {
      return &deep_cm_cache[i];
    }
  }

  if (deep_cm_n >= deep_cm_cap) {
    deep_cm_cap = MAX(64, deep_cm_cap * 2);
    deep_cm_cache = fc_realloc(deep_cm_cache,
                               deep_cm_cap * sizeof(*deep_cm_cache));
  }
  e = &deep_cm_cache[deep_cm_n++];
  e->city_id = pcity->id;
  e->valid = FALSE;
  e->cost = 0.0;
  e->alt = deep_cm_peace_posture(ait, pcity);
  output_type_iterate(o) {
    e->war[o] = 0;
    e->peace[o] = 0;
  } output_type_iterate_end;

  deep_cm_base(&war, pcity);
  deep_cm_apply_posture(&war, pcity, DEEP_CM_WAR);

  cm_init_parameter(&peace);
  if (e->alt == DEEP_CM_NO_POSTURE) {
    /* No deep posture: the city would run the stock default, and that
     * is what the footing must be priced against. */
    set_default_city_manager(&peace, pcity);
  } else {
    deep_cm_base(&peace, pcity);
    deep_cm_apply_posture(&peace, pcity, e->alt);
  }

  clock = timer_new(TIMER_CPU, TIMER_ACTIVE, "deep-cm");
  timer_start(clock);
  cm_clear_cache(pcity);
  res = cm_result_new(pcity);
  cm_query_result(pcity, &war, res, FALSE);
  if (res->found_a_valid) {
    output_type_iterate(o) {
      e->war[o] = res->surplus[o];
    } output_type_iterate_end;
    cm_query_result(pcity, &peace, res, FALSE);
    if (res->found_a_valid) {
      output_type_iterate(o) {
        e->peace[o] = res->surplus[o];
      } output_type_iterate_end;
      e->valid = TRUE;
    }
  }
  cm_result_destroy(res);
  timer_stop(clock);
  deep_cm_seconds += timer_read_seconds(clock);
  timer_destroy(clock);
  deep_cm_solves += 2;

  if (e->valid && e->peace[O_FOOD] > 0) {
    /* The charge is the fraction of the growth RATE the footing gives
     * up, not an absolute food count: a city banking 8 food that drops
     * to 6 has given up a quarter of its growth, and so has a city
     * banking 4 that drops to 3. Clipped to [0, 1] -- a footing that
     * gains food charges nothing, and one that stops growth entirely
     * charges the whole knob. A city not growing under the
     * alternative either forgoes nothing (surplus <= 0). */
    double lost = (double) (e->peace[O_FOOD] - e->war[O_FOOD])
                  / (double) e->peace[O_FOOD];

    e->cost = CLIP(0.0, lost, 1.0);
  }

  return e;
}

/**********************************************************************//**
  Derive and set (or clear) cm_parameter for every owned city.
**************************************************************************/
void deep_set_city_manager_params(struct ai_type *ait,
                                  struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct cm_parameter cmp;
    enum deep_cm_posture posture;
    bool matched;
    bool threatened;

    deep_cm_base(&cmp, pcity);

    /* "Under threat" has to mean the city could FALL, not that an enemy
     * is standing near it. urgency and grave_danger are presence COUNTS
     * -- grave_danger increments for any capture-capable unit within one
     * turn's move, urgency within three -- and neither asks whether the
     * garrison wins, so a city behind walls with five Musketeers scores
     * what an empty one scores. The same substitution was corrected in
     * three other places on 2026-08-01; this was the fourth site, and
     * the costliest of them, for two reasons.
     *
     * It is the FIRST branch of this state machine, so growth-capped,
     * wonder and grow-hard are all UNREACHABLE for a city that merely
     * has a neighbour. And Europe T106 measured urgency > 0 on 69% of
     * deep's cities during a war -- which left cm_cap_*, cm_grow_* and
     * cm_wonder_* governing a minority of cities while the tuning
     * campaign spent its budget optimising them.
     *
     * The threshold is cm_war_survival and NOT survival_target, even
     * though both ask "will this city hold". survival_target already
     * arbitrates some twenty decisions across the defence and operations
     * layers -- garrison lending, worker guards, brigade relay, relief
     * sizing -- where being wrong loses a city. Being wrong here loses
     * some food. Those want different answers, and one scalar cannot
     * give both; sharing it would also put ECON and MIL in a tug of war
     * over a single number, which is the block coupling machine B's
     * alternation table exists to detect. Fall back to the stock flags
     * only when the gate is switched off entirely. */
    if (tune->cm_war_survival > 0.0) {
      int wave = 0;
      /* One verdict per city per phase (deepcombat.c): the growth
       * ladder at the end of this turn reads this same probability
       * against survival_target. The thresholds stay separate for the
       * reason argued above; the COMPUTATION does not. */
      double surv = deep_city_survival_verdict(&(wld.map), pcity,
                                               tune->survival_horizon,
                                               &wave);

      threatened = wave > 0 && surv < tune->cm_war_survival;

      if (threatened && tune->cm_war_growth_charge > 0.0) {
        /* The footing is no longer free. Solve both arrangements, find
         * what the shields actually cost in growth, and lower the bar
         * the danger has to clear by that much: a city that gives up
         * its whole food surplus to arm has to be in real trouble
         * before it does, while a city that loses nothing flips on the
         * unchanged threshold.
         *
         * Deliberately evaluated INSIDE the free-flip test rather than
         * before it, for two reasons. The charge can only ever revoke
         * a flip -- it is a cost, and a cost does not create demand --
         * so a city the gate was not going to flip needs no price. And
         * the pair of solves then runs on the flip candidates alone,
         * which is what keeps the solver bill proportional to the
         * decisions it informs instead of to the empire. */
        const struct deep_cm_memo *m = deep_cm_counterfactual(ait, pcity);
        double bar = tune->cm_war_survival
                     - tune->cm_war_growth_charge * m->cost;

        if (m->valid && surv >= bar) {
          threatened = FALSE;
        }
        log_base(deep_metric_log_level(),
                 "deep-cmpost turn=%d player=\"%s\" city=%d size=%d"
                 " surv=%.3f bar=%.3f cost=%.3f alt=%d valid=%d"
                 " warf=%d wars=%d wart=%d altf=%d alts=%d altt=%d"
                 " kept=%d",
                 game.info.turn, player_name(pplayer), pcity->id,
                 city_size_get(pcity), surv, bar, m->cost, (int) m->alt,
                 m->valid ? 1 : 0,
                 m->war[O_FOOD], m->war[O_SHIELD], m->war[O_TRADE],
                 m->peace[O_FOOD], m->peace[O_SHIELD], m->peace[O_TRADE],
                 threatened ? 1 : 0);
      }
    } else {
      threatened = city_data->urgency > 0 || city_data->grave_danger > 0;
    }

    posture = (threatened ? DEEP_CM_WAR
                          : deep_cm_peace_posture(ait, pcity));
    matched = deep_cm_apply_posture(&cmp, pcity, posture);

    if (matched) {
      if (pcity->cm_parameter == NULL) {
        pcity->cm_parameter = fc_malloc(sizeof(*pcity->cm_parameter));
        cm_init_parameter(pcity->cm_parameter);
      }
      if (!cm_are_parameter_equal(pcity->cm_parameter, &cmp)) {
        /* The rule for this city changed; rearrange now rather than
         * waiting for the next organic rearrangement, or the new
         * priorities sit unused for many turns. */
        cm_copy_parameter(pcity->cm_parameter, &cmp);
        auto_arrange_workers(pcity);
      }
    } else if (pcity->cm_parameter != NULL) {
      /* Back to the stock default for this city. */
      free(pcity->cm_parameter);
      pcity->cm_parameter = NULL;
      auto_arrange_workers(pcity);
    }
  } city_list_iterate_end;

  if (tune->cm_war_growth_charge > 0.0) {
    /* The bill, in public: the counterfactual doubles a CM evaluate for
     * every city it prices, and a claim that the cost is acceptable is
     * worth nothing without the number beside it. Cumulative for the
     * turn across every deep player, so the LAST player's line is the
     * turn's total. */
    log_base(deep_metric_log_level(),
             "deep-cmsolve turn=%d player=\"%s\" solves=%d cpu=%.6f",
             game.info.turn, player_name(pplayer), deep_cm_solves,
             deep_cm_seconds);
  }
}
