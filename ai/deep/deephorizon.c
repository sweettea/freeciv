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
#include "extras.h"
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
#include "advbuilding.h"
#include "advdata.h"
#include "advtools.h"
#include "infracache.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daiplayer.h"

#include "deepai.h"
#include "deepcombat.h"
#include "deepstance.h"
#include "deeptuning.h"

#include "deepwonder.h"

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

/* Horizon credits whose planning horizon the bell actually shortened
 * during the current player's build pass, and how short it got. Not
 * saved and not per-player state: the pass resets them and logs them,
 * which is the whole census. */
static int deep_horizon_clamped = 0;
static int deep_horizon_missed = 0;

/**********************************************************************//**
  Turns of play left after this one.

  `game.server.end_turn` is plain readable game state, and before this
  commit NOTHING in ai/ or server/advisors/ read it -- the whole AI
  plans as though the game were infinite, on cells where 199 of 199
  games end on the turn limit. Returns DEEP_NO_END_TURN when no bell is
  set, so callers can MIN() unconditionally.
**************************************************************************/
int deep_turns_left(void)
{
  if (game.server.end_turn <= 0) {
    return DEEP_NO_END_TURN;
  }

  return MAX(0, game.server.end_turn - game.info.turn);
}

/**********************************************************************//**
  The planning horizon the growth ladder may count over: MORT, cut down
  to the turns that remain.

  Every credit in this file counts a per-turn value across MORT turns
  and discounts once for the delay, and the upkeep bill is charged over
  the same MORT. On turn 195 of 200 that prices 24 turns of payback
  that do not exist -- and prices 24 turns of upkeep that will not be
  paid either, which is why the clamp is applied to BOTH sides here and
  not only to the benefit.

  Off by default (`endgame_horizon = 0`, byte-identity), and inert
  even when on until the game is inside MORT turns of its end, which is
  what makes "the clamp cannot regress mid-game behaviour" a fact about
  the code rather than a hope about the numbers.
**************************************************************************/
int deep_horizon_turns(void)
{
  int left;

  if (deep_tuning_get()->endgame_horizon <= 0) {
    return MORT;
  }

  left = deep_turns_left();
  if (left >= MORT) {
    return MORT;
  }

  deep_horizon_clamped++;
  return left;
}

/**********************************************************************//**
  Whether a benefit whose first instalment arrives in `delay` turns
  arrives at all.

  The horizon clamp above shortens what a credit counts; this one is
  the other half, and the two are genuinely different. A Granary in a
  city that grows every 30 turns still earns `MAX(1, H/ttg)` growth
  steps under the clamp because the formula floors at one -- which is
  right when the game has 200 turns to run and wrong when it has five.
  Always TRUE with the clamp off, so the floor's existing behaviour is
  untouched at defaults.
**************************************************************************/
bool deep_arrives_in_time(int delay)
{
  if (deep_tuning_get()->endgame_horizon <= 0) {
    return TRUE;
  }
  if (delay <= deep_turns_left()) {
    return TRUE;
  }

  deep_horizon_missed++;
  return FALSE;
}

/**********************************************************************//**
  Whether the game is inside its final `endgame_window` turns.

  This is a different question from the horizon clamp's, and the two
  are deliberately measured apart. The clamp is arithmetic: it fires
  whenever the remaining turns are fewer than the horizon the credit
  was counting, needs no window, and is correctness. The window is
  DOCTRINE: inside it deep stops playing for the empire and starts
  playing for the scoreboard, and how many turns that is worth is a
  measured quantity, not a derived one.
**************************************************************************/
bool deep_in_endgame_window(void)
{
  int window = deep_tuning_get()->endgame_window;

  return window > 0 && deep_turns_left() < window;
}

/**********************************************************************//**
  Whether `target` can actually be COMPLETED before the last turn is
  played.

  Production is resolved in the end-of-turn phase, so the turns that
  still deliver a completed build are this one and every one after it:
  `end_turn - turn + 1`. city_turns_to_build() already accounts for the
  change-production penalty this seat now pays (HEDGE), so a switch
  that forfeits half the box is correctly reported as slower.

  What it does NOT model is a caravan arriving to Help Wonder, so a
  wonder a fleet of caravans would finish reads as unfinishable here.
  That is a known under-count and it is conservative in the direction
  that matters: it can only decline to pay a wonder, never overpay one.
**************************************************************************/
bool deep_finishes_in_time(const struct city *pcity,
                           const struct universal *target)
{
  return city_turns_to_build(pcity, target, TRUE) <= deep_turns_left() + 1;
}

/**********************************************************************//**
  Mean weighted-output value of the best currently-unworked workable
  tiles of the city -- what a new citizen would earn, in the same
  currency as the tile sums (W02).

  When `raw` is non-null it is filled with the mean RAW output of those
  same tiles, per output type, so a caller that needs to know what the
  arriving citizen produces (rather than only what it is worth) reads
  the identical tile selection. Tiles yield food, shield and trade only;
  the other outputs stay zero here and are derived by the caller.
**************************************************************************/
adv_want deep_unworked_tile_value(struct city *pcity,
                                  struct adv_data *adv, int count,
                                  int *raw)
{
  const struct civ_map *nmap = &(wld.map);
  int best[3] = {0, 0, 0};
  int bestraw[3][O_LAST];
  int n = 0, i;
  adv_want total = 0;

  fc_assert_ret_val(count <= 3, 0);

  memset(bestraw, 0, sizeof(bestraw));
  if (raw != NULL) {
    memset(raw, 0, O_LAST * sizeof(*raw));
  }

  city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                    city_tile(pcity), ptile) {
    int sum;
    int out[O_LAST];

    if (tile_worked(ptile) != NULL || ptile == city_tile(pcity)
        || !city_can_work_tile(pcity, ptile)) {
      continue;
    }
    memset(out, 0, sizeof(out));
    out[O_FOOD] = city_tile_output(pcity, ptile, FALSE, O_FOOD);
    out[O_TRADE] = city_tile_output(pcity, ptile, FALSE, O_TRADE);
    out[O_SHIELD] = city_tile_output(pcity, ptile, FALSE, O_SHIELD);
    sum = out[O_FOOD] * adv->food_priority
          + out[O_TRADE] * adv->science_priority
          + out[O_SHIELD] * adv->shield_priority;
    for (i = 0; i < 3; i++) {
      if (sum > best[i]) {
        int t = best[i];
        int tout[O_LAST];

        /* Carry the displaced tile's outputs down with its score, so
         * bestraw[] stays the breakdown of exactly the tiles best[]
         * scored. */
        memcpy(tout, bestraw[i], sizeof(tout));
        best[i] = sum;
        memcpy(bestraw[i], out, sizeof(out));
        sum = t;
        memcpy(out, tout, sizeof(out));
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
    if (raw != NULL) {
      output_type_iterate(o) {
        raw[o] += bestraw[i][o];
      } output_type_iterate_end;
    }
  }
  if (raw != NULL) {
    output_type_iterate(o) {
      raw[o] /= count;
    } output_type_iterate_end;
  }
  return total / count;
}

/**********************************************************************//**
  Descending integer order, for the counterfactual food arrangement.
**************************************************************************/
static int deep_int_desc(const void *a, const void *b)
{
  return *(const int *) b - *(const int *) a;
}

/**********************************************************************//**
  How many turns this city would take to grow IF the ruleset's size gate
  were lifted -- the question the capacity credit needs answered, and
  the one city_turns_to_grow() structurally cannot answer for a capped
  city.

  In a capped city the observed ttg is not a growth rate, it is an
  artefact of the cap. city_increase_size() refills the food box to
  granary x (1 - aqueductloss...) and GAME_DEFAULT_AQUEDUCTLOSS is 0,
  which no benchmark config overrides, so the box stays pinned exactly
  full and city_turns_to_grow() returns 0 by integer division forever.
  Independently, deep's own CM runs its capped state (food factor
  cm_cap_food against shield and trade at cm_cap_*) with a food floor
  of 0 once the box is full, so the arrangement correctly sheds food it
  cannot spend, the surplus goes to 0, and ttg becomes FC_INFINITY. The
  credit then read back "the cap is not the binding constraint" in
  exactly the city whose only binding constraint was the cap -- and
  because the whole city is skipped when threatened && cap_credit <= 0,
  a long-capped city under any threat got no horizon credits at all.
  That is two of the three causes of the Europe T106 evidence (17 of 19
  cities at the cap, zero Aqueducts) that STATUS attributed wholly to
  the war gate.

  The counterfactual is computed from the CATCHMENT rather than from
  the arrangement, which is what breaks the deadlock: it cannot be
  zeroed by the CM's decision to stop collecting food, so the CM's cap
  state needs no change and keeps shedding food the capped city genuinely
  cannot use. The city's citizens are placed on its best food tiles and
  its live food usage (citizens plus unit upkeep) is charged against the
  total. If that surplus is still not positive the cap really is not what
  is stopping this city, and the credit stays zero.

  Returns the growth interval in turns, or 0 for "would not grow";
  surplus_out (optional) reports the counterfactual food surplus.
**************************************************************************/
static int deep_counterfactual_ttg(struct city *pcity, int *surplus_out)
{
  const struct civ_map *nmap = &(wld.map);
  int radius_sq = city_map_radius_sq_get(pcity);
  int food[city_map_tiles(radius_sq)];
  int n = 0, i, surplus;
  int total = city_tile_output(pcity, city_tile(pcity), FALSE, O_FOOD);
  /* Every citizen on a tile. Reading city_specialists() back would
   * reimport the very state this function exists to see past: the cap
   * state weights food at cm_cap_food against shield and trade at
   * cm_cap_*, so a specialist beats a food tile and the CM parks
   * citizens off the land BECAUSE the city is capped. Measured on
   * isolated-150, that read the credit back to zero in 29 of the 34
   * capped city-turns where the CM had shed its food -- the exact
   * deadlock. The happiness ceiling is real, but it is the SOFT cap
   * and it has its own credit (deep_content_credit); charging it here
   * as well would price it twice and would leave the hard cap unpriced
   * in every unhappy city. */
  int workers = city_size_get(pcity);

  city_tile_iterate(nmap, radius_sq, city_tile(pcity), ptile) {
    if (ptile == city_tile(pcity) || !city_can_work_tile(pcity, ptile)) {
      continue;
    }
    food[n++] = city_tile_output(pcity, ptile, FALSE, O_FOOD);
  } city_tile_iterate_end;

  qsort(food, n, sizeof(food[0]), deep_int_desc);
  for (i = 0; i < n && i < workers; i++) {
    total += food[i];
  }

  surplus = total - pcity->usage[O_FOOD];
  if (surplus_out != NULL) {
    *surplus_out = surplus;
  }
  if (surplus <= 0) {
    return 0;
  }

  /* A full growth cycle at that surplus. The box of a capped city is
   * pinned full, so the FIRST citizen would arrive the turn the gate
   * lifted; this is the rate at which the ones after it arrive, which
   * is what the horizon is counting. */
  return MAX(1, (city_granary_size(city_size_get(pcity)) + surplus - 1)
                / surplus);
}

/**********************************************************************//**
  Credit for a building that lifts this city's growth ceiling: the
  blocked growth curve over the planning horizon (H = MORT, the
  documented amortize horizon), valued at what new citizens would earn
  from the best unworked tiles, amortized by the time-to-grow the city
  would have if the ceiling were lifted.
**************************************************************************/
static adv_want deep_capacity_credit(struct city *pcity,
                                     struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int ttg, cfsurplus = 0;
  int steps;
  adv_want per_citizen, credit = 0;

  if (city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
    /* Not currently capped; the cap-lifter earns only anticipation. */
    return 0;
  }

  /* Capped. One census line per capped city per turn, credit or no
   * credit: "the mechanism fired" and "the mechanism paid" are
   * different claims and only the counter tells them apart. */
  ttg = deep_counterfactual_ttg(pcity, &cfsurplus);
  if (ttg > 0 && deep_arrives_in_time(ttg)) {
    steps = MAX(1, deep_horizon_turns() / ttg);
    per_citizen = deep_unworked_tile_value(pcity, adv, 3, NULL);
    credit = amortize(per_citizen * steps * tune->horizon_capacity_weight,
                      ttg);
  }

  log_base(deep_metric_log_level(),
           "deep-capacity turn=%d player=\"%s\" city=%d size=%d"
           " spec=%d obsttg=%d obssurplus=%d cfttg=%d cfsurplus=%d"
           " credit=%.1f",
           game.info.turn, player_name(city_owner(pcity)), pcity->id,
           city_size_get(pcity), city_specialists(pcity),
           city_turns_to_grow(pcity), pcity->surplus[O_FOOD], ttg,
           cfsurplus, (double) credit);

  return credit;
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
int deep_pacification_load(const struct city *pcity)
{
  return pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL]
         + deep_entertainer_load(pcity);
}

/**********************************************************************//**
  The half of that load which is actually OFF THE TILES: citizens the
  arrangement turned into entertainer-equivalent specialists to keep
  order. An unhappy citizen still works its tile and still produces --
  it costs disorder RISK, not output -- so anything pricing "what
  luxury hands back to the land" wants this number and not the sum.
**************************************************************************/
int deep_entertainer_load(const struct city *pcity)
{
  int load = 0;

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
  arriving = (ttg > 0 ? deep_horizon_turns() / ttg : 0);

  /* A building can only ever convert its own `amount`. */
  freed = MIN(amount, load + arriving);
  if (freed <= 0) {
    return 0;
  }

  if (load <= 0 && !deep_arrives_in_time(ttg)) {
    /* Nothing to pacify today and nobody arriving before the bell. */
    return 0;
  }

  per_citizen = deep_unworked_tile_value(pcity, adv, 3, NULL);

  /* Immediate if the load is already there; otherwise it starts paying
   * when the city next grows. */
  return amortize(freed * per_citizen * tune->horizon_content_weight,
                  load > 0 ? 0 : ttg);
}

/**********************************************************************//**
  Credit for a growth ACCELERATOR (Growth_Food: the Granary keeps a
  share of the food box across a growth, so the next citizen arrives
  sooner). Valued as the extra citizens the horizon buys, in the same
  currency and over the same MORT horizon as the other credits.

  This deliberately does NOT reuse the capacity credit. The two answer
  opposite questions, and conflating them inverted the accelerator:
  deep_capacity_credit() is nonzero only when `!city_can_grow_to(size+1)`
  -- when the city is jammed against the ruleset's size gate -- so
  gating the Growth_Food credit on `cap_credit > 0` paid a growth
  accelerator EXACTLY WHEN GROWTH WAS IMPOSSIBLE, and paid it nothing in
  the small, freely-growing city where a Granary actually earns. A
  Granary in a capped city saves food the city cannot spend.
**************************************************************************/
static adv_want deep_growth_credit(struct city *pcity,
                                   const struct impr_type *pimprove,
                                   struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int pct, ttg, steps;
  adv_want per_citizen, extra;

  pct = get_potential_improvement_bonus(pimprove, pcity, EFT_GROWTH_FOOD,
                                        RPT_CERTAIN, FALSE);
  if (pct <= 0) {
    return 0;
  }
  if (!city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
    /* Capped: the saved food has nowhere to go. The cap-lifter is the
     * building this city needs, and it has its own credit. */
    return 0;
  }
  ttg = city_turns_to_grow(pcity);
  if (ttg <= 0) {
    /* No food surplus: nothing to accelerate. */
    return 0;
  }

  if (!deep_arrives_in_time(ttg)) {
    /* The city does not grow again before the bell; there is no growth
     * left to accelerate. */
    return 0;
  }

  /* Growths within the horizon, and the share of one growth-cycle's
   * food the building hands back each time. */
  steps = MAX(1, deep_horizon_turns() / ttg);
  extra = (adv_want) steps * pct / 100.0;
  per_citizen = deep_unworked_tile_value(pcity, adv, 3, NULL);

  return amortize(extra * per_citizen * tune->horizon_growth_weight, ttg);
}

/**********************************************************************//**
  Percentage output bonuses pimprove would add to pcity, per output
  type. Output_Bonus effects carry an OutputType requirement at Local
  range naming which output they scale, so the answer is a vector and
  not the scalar get_potential_improvement_bonus() would return -- that
  one filters on effect type alone and would fold a Marketplace's +50%
  gold and +50% luxury into a meaningless 100.

  The OutputType requirement is evaluated against the output being
  computed rather than against a city, so it is read here rather than
  passed to is_req_active(); every OTHER requirement must be active,
  which is the same discipline get_potential_improvement_bonus() uses.
**************************************************************************/
static void deep_output_bonus_pct(const struct city *pcity,
                                  const struct impr_type *pimprove,
                                  int *pct)
{
  struct universal source = {.kind = VUT_IMPROVEMENT,
                             .value = {.building = pimprove}};
  struct effect_list *plist = get_req_source_effects(&source);
  const struct req_context context = {
    .player = city_owner(pcity),
    .city = pcity,
    .building = pimprove,
  };

  memset(pct, 0, O_LAST * sizeof(*pct));
  if (plist == NULL) {
    return;
  }

  effect_list_iterate(plist, peffect) {
    Output_type_id otype = O_LAST;
    bool useful = TRUE;

    if (peffect->type != EFT_OUTPUT_BONUS
        && peffect->type != EFT_OUTPUT_BONUS_2) {
      continue;
    }
    requirement_vector_iterate(&peffect->reqs, preq) {
      if (preq->source.kind == VUT_OTYPE && preq->present) {
        otype = preq->source.value.outputtype;
        continue;
      }
      if (preq->source.kind == VUT_IMPROVEMENT
          && preq->source.value.building == pimprove) {
        /* The building itself: that is what we are pricing. */
        if (!preq->present) {
          useful = FALSE;
          break;
        }
        continue;
      }
      if (!is_req_active(&context, nullptr, preq, RPT_CERTAIN)) {
        useful = FALSE;
        break;
      }
    } requirement_vector_iterate_end;

    if (useful && otype >= 0 && otype < O_LAST) {
      pct[otype] += peffect->value;
    }
  } effect_list_iterate_end;
}

/**********************************************************************//**
  Credit for a MULTIPLIER building -- one that scales an output by a
  percentage (Marketplace, Library, Bank, Factory).

  These are the one column of the ladder stock DOES price: they are
  ADV_IMPR_CALCULATE, so base_want() runs. But dai_city_want() measures
  them against `acity->citizen_base` -- the output of the tiles this
  city works TODAY -- while every other credit in this file is buying
  the citizens that will work MORE tiles tomorrow. Nothing told the
  multiplier about the growth being purchased for it, so deep bought
  the growth and then declined the buildings that would compound it:
  20 Marketplaces to baseline's 108 on the 250-turn cell, at the same
  city density and a HIGHER trade per city.

  Priced as the arriving citizens' output, run through the building's
  own percentage, valued at the advisor's own output priorities. Using
  adv->*_priority rather than a flat rate matters: trade splits into
  gold, luxury and science by the current tax rates, and if this
  empire has no use for gold then the gold half of a Marketplace is
  correctly worth little without that having to be asserted here.
**************************************************************************/
static adv_want deep_multiplier_credit(struct city *pcity,
                                       const struct impr_type *pimprove,
                                       struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct player *pplayer = city_owner(pcity);
  int pct[O_LAST], raw[O_LAST];
  int ttg, arriving, any = 0;
  adv_want credit = 0, trade;

  deep_output_bonus_pct(pcity, pimprove, pct);
  output_type_iterate(o) {
    any += pct[o];
  } output_type_iterate_end;
  if (any <= 0) {
    return 0;
  }

  if (!city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
    /* Capped: no citizens are arriving for the multiplier to act on.
     * The cap-lifter is what this city needs and it has its own
     * credit. */
    return 0;
  }
  ttg = city_turns_to_grow(pcity);
  if (ttg <= 0) {
    /* No food surplus: nothing arriving. */
    return 0;
  }
  arriving = deep_horizon_turns() / ttg;
  if (arriving <= 0) {
    /* Under the endgame clamp this is the "nobody arrives before the
     * bell" case, and it needs no separate test: the horizon itself
     * has already gone below one growth cycle. */
    return 0;
  }

  (void) deep_unworked_tile_value(pcity, adv, 3, raw);

  /* A tile yields food, shield and trade; trade becomes gold, luxury
   * and science at the current rates, which is the same conversion
   * dai_city_want() performs via add_tax_income(). */
  trade = (adv_want) arriving * raw[O_TRADE];
  credit += pct[O_FOOD] / 100.0
            * arriving * raw[O_FOOD] * adv->food_priority;
  credit += pct[O_SHIELD] / 100.0
            * arriving * raw[O_SHIELD] * adv->shield_priority;
  credit += pct[O_TRADE] / 100.0 * trade * adv->science_priority;
  credit += pct[O_GOLD] / 100.0
            * trade * pplayer->economic.tax / 100.0 * adv->gold_priority;
  credit += pct[O_LUXURY] / 100.0
            * trade * pplayer->economic.luxury / 100.0
            * adv->luxury_priority;
  credit += pct[O_SCIENCE] / 100.0
            * trade * pplayer->economic.science / 100.0
            * adv->science_priority;

  if (credit <= 0) {
    return 0;
  }

  return amortize(credit * tune->horizon_multiplier_weight, ttg);
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
  const struct player *pplayer = city_owner(pcity);
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

      if (has_this_building && partner != NULL
          /* Somebody has to be able to lay the partner extra down. An
           * extra that appears from resources, huts or terrain change
           * is not a bundle this empire can decide to complete. */
          && is_extra_caused_by_worker_action(partner)) {
        int viable = 0;

        city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                          city_tile(pcity), ptile) {
          /* A tile counts only if a worker of OURS could actually put
           * the partner there: terrain that accepts it, the extra's own
           * requirements (its tech among them) satisfiable, and no
           * conflicting extra already down. Before this the credit
           * counted a tile for merely not refusing the extra, which on
           * a normal catchment is nearly every tile -- so deep paid
           * effect_value x ~20 x weight for half of a bundle it had no
           * mechanism and sometimes no technology to finish. The worker
           * side of the pairing is W30's; the CREDIT was never deferred
           * with it. */
          if (ptile != city_tile(pcity)
              && !tile_has_conflicting_extra(ptile, partner)
              && (tile_has_extra(ptile, partner)
                  || player_can_build_extra(partner, pplayer, ptile))) {
            viable++;
          }
        } city_tile_iterate_end;

        credit += peffect->value * viable * tune->horizon_bundle_weight;
      }
    } effect_list_iterate_end;
  }

  return credit;
}

/* DARKFX: how many cities the gov-centre credit found savings in
 * during the current build pass, and the total credit it paid. The
 * execution counter for a credit whose whole claim is "stock cannot
 * see this at all". */
static int deep_govcenter_cities = 0;
static adv_want deep_govcenter_paid = 0;

/**********************************************************************//**
  The waste percentage the ruleset charges `pcity` on output `o` when
  its nearest government centre is `dist` tiles away -- or when there
  is no government centre anywhere, which the ruleset charges as total
  loss.

  This mirrors the distance arm of city_waste() (common/city.c:3345-
  3386) rather than calling it, because the two things it needs are not
  reachable: nearest_gov_center() is static, and city_waste() reads the
  centres that exist NOW, so there is no way to ask it the
  counterfactual question "what would this city lose if the nearest
  centre were somewhere else".

  Returned as an effective percentage: the flat waste level and the
  distance term are summed and clipped exactly as the server clips
  them, then reduced by EFT_OUTPUT_WASTE_PCT, so the difference between
  two calls is the real saving and not just the distance term. The one
  thing it does not reproduce is the notradesize/fulltradesize size
  penalty, which is independent of distance and cancels in the
  difference.
**************************************************************************/
static int deep_waste_pct_at(const struct city *pcity, Output_type_id o,
                             int dist, bool have_center)
{
  const struct output_type *output = get_output_type(o);
  int by_dist = get_city_output_bonus(pcity, output,
                                      EFT_OUTPUT_WASTE_BY_DISTANCE);
  int by_rel = get_city_output_bonus(pcity, output,
                                     EFT_OUTPUT_WASTE_BY_REL_DISTANCE);
  int level = get_city_output_bonus(pcity, output, EFT_OUTPUT_WASTE);
  int waste_pct = get_city_output_bonus(pcity, output,
                                        EFT_OUTPUT_WASTE_PCT);

  if (by_dist <= 0 && by_rel <= 0) {
    /* Distance costs this output nothing, so a centre saves it
     * nothing either -- and the "no centre at all" total loss does not
     * apply, because the server only reaches it through this test. */
    return 0;
  }
  if (!have_center) {
    level = 100;
  } else {
    level += by_dist * dist / 100;
    if (by_rel > 0) {
      level += by_rel * 50 * dist / 100
               / MAX(MAP_NATIVE_WIDTH, MAP_NATIVE_HEIGHT);
    }
  }
  level = CLIP(0, level, 100);

  return level - level * waste_pct / 100;
}

/**********************************************************************//**
  Whether `gcity` stops being a government centre the moment `pimprove`
  is built somewhere else.

  This is the difference between adding a centre and MOVING one. The
  Palace and civ2civ3's Ecclesiastical Palace are both one per player,
  so building either in a new city destroys the existing one and its
  city reverts to being a distant province like any other. A city that
  holds this same improvement therefore does not count among the
  centres that survive the decision.
**************************************************************************/
static bool deep_center_lost_to(const struct city *gcity,
                                const struct impr_type *pimprove)
{
  return city_has_building(gcity, pimprove);
}

/**********************************************************************//**
  Credit a government centre with the distance waste it saves the REST
  of the empire. Returns the credit for pimprove in pcity, or 0.

  This is the effects audit's first ranked opportunity. EFT_GOV_CENTER
  is listed in daieffects.c:176 as "already evaluated in base_want()"
  and is not; base_want() runs only for advdata.c's eleven trigger
  effects and GOV_CENTER is not one of them, so the civ2civ3
  Ecclesiastical Palace -- 140 shields, and worth a fifth of the trade
  of every distant city -- has literally no want beyond its History
  WAG. And a City-range requirement would misprice it even if
  base_want() did run, because the benefit is not in the city that
  builds it.

  The counterfactual is the ruleset's own arithmetic and nothing else:
  for every other city, its nearest centre moves from wherever it is to
  this candidate if that is closer, and the waste percentage the
  ruleset then charges falls by a computable amount. There is no WAG in
  the term; the only free number is the weight.
**************************************************************************/
static adv_want deep_gov_center_credit(struct city *pcity,
                                       const struct impr_type *pimprove,
                                       struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  const struct tile *ctile = city_tile(pcity);
  bool have_center = FALSE;
  adv_want credit = 0;
  double trade_value;

  if (tune->horizon_govcenter_weight <= 0.0) {
    return 0;
  }
  if (get_potential_improvement_bonus(pimprove, pcity, EFT_GOV_CENTER,
                                      RPT_CERTAIN, FALSE) <= 0) {
    return 0;
  }
  if (is_gov_center(pcity)) {
    /* A second centre in a city that already is one saves nobody
     * anything: every other city's distance is unchanged. */
    return 0;
  }

  /* THE COUNTERFACTUAL HAS TO MODEL THE MOVE, NOT A SECOND COPY. A
   * Palace and an Ecclesiastical Palace are one per player, so
   * building either HERE destroys the one over there and its city
   * reverts to being a distant province. Pricing the addition while
   * leaving the old copy standing made every city in the empire a
   * permanent Palace candidate: 3454 firings a game on isolated-150 at
   * a mean credit of 2827 against a whole-ladder mean of 85, and the
   * arm lost 49.4 score with 2.8 fewer cities (0/20 seeds).
   *
   * So there are two states, and the difference between them is the
   * whole term: TODAY, every centre that exists; AFTER, the centres
   * that survive the move plus this candidate. The city that loses the
   * improvement is charged the waste it starts paying, which is what
   * makes a relocation that helps nobody correctly worthless. */
  city_list_iterate(pplayer->cities, gcity) {
    if (is_gov_center(gcity)) {
      have_center = TRUE;
    }
  } city_list_iterate_end;

  /* A point of trade becomes gold, luxury and science at the current
   * rates -- the same conversion deep_multiplier_credit() performs,
   * which is the same one dai_city_want() performs via
   * add_tax_income(). */
  trade_value = (pplayer->economic.tax * adv->gold_priority
                 + pplayer->economic.luxury * adv->luxury_priority
                 + pplayer->economic.science * adv->science_priority)
                / 100.0;

  city_list_iterate(pplayer->cities, ocity) {
    int now = FC_INFINITY, then;
    adv_want saved = 0;

    if (ocity == pcity) {
      continue;
    }
    if (is_gov_center(ocity) && !deep_center_lost_to(ocity, pimprove)) {
      /* Stays a centre either way: distance zero before and after. */
      continue;
    }
    if (is_gov_center(ocity)) {
      now = 0;
    } else {
      city_list_iterate(pplayer->cities, gcity) {
        if (is_gov_center(gcity)) {
          now = MIN(now, real_map_distance(city_tile(ocity),
                                           city_tile(gcity)));
        }
      } city_list_iterate_end;
    }
    then = real_map_distance(city_tile(ocity), ctile);
    city_list_iterate(pplayer->cities, gcity) {
      if (is_gov_center(gcity) && !deep_center_lost_to(gcity, pimprove)) {
        then = MIN(then, real_map_distance(city_tile(ocity),
                                           city_tile(gcity)));
      }
    } city_list_iterate_end;
    if (have_center && then == now) {
      continue;
    }

    output_type_iterate(o) {
      int total, dpct;

      if (o != O_FOOD && o != O_SHIELD && o != O_TRADE) {
        /* Only the three tile outputs carry distance waste; gold,
         * luxury and science are derived from trade after it. */
        continue;
      }
      /* prod[] has already had waste taken off it (city_refresh()),
       * so the pre-waste output the percentages apply to is the sum
       * of the two. */
      total = ocity->prod[o] + ocity->waste[o];
      if (total <= 0) {
        continue;
      }
      /* Signed on purpose: `then` can be FURTHER than `now` for the
       * city the improvement is being moved out of, and that debit is
       * the price of the move. */
      dpct = deep_waste_pct_at(ocity, o, now, have_center)
             - deep_waste_pct_at(ocity, o, then, TRUE);
      if (dpct == 0) {
        continue;
      }
      saved = (adv_want) total * dpct / 100.0;
      switch (o) {
      case O_FOOD:
        credit += saved * adv->food_priority;
        break;
      case O_SHIELD:
        credit += saved * adv->shield_priority;
        break;
      case O_TRADE:
        credit += saved * trade_value;
        break;
      default:
        break;
      }
    } output_type_iterate_end;
  } city_list_iterate_end;

  if (credit <= 0) {
    return 0;
  }

  /* Per-turn saving over the planning horizon, in the same dimension
   * as the upkeep bill this credit has to beat (which is charged as
   * gold-per-turn x deep_horizon_turns()). The benefit starts the turn
   * the building finishes and does not wait on a growth step, so there
   * is no delay to amortize -- the same call the plague term makes. */
  credit *= deep_horizon_turns() * tune->horizon_govcenter_weight;
  deep_govcenter_cities++;
  deep_govcenter_paid += credit;

  return credit;
}

/* DARKFX: the free-upkeep credit's execution counter -- candidates
 * that found a real bill to cancel, and what they were paid. */
static int deep_upkeepfree_cands = 0;
static adv_want deep_upkeepfree_paid = 0;

/**********************************************************************//**
  Credit a PLAYER-range free-upkeep effect at the empire it acts on.
  Returns the credit for pimprove in pcity, or 0.

  EFT_UNIT_UPKEEP_FREE_PER_CITY sits in daieffects.c's "currently not
  supported for building AI" list, so King Richard's Crusade (2 free
  gold of unit upkeep in every city) and the Statue of Zeus (1 free
  shield, likewise) are valued at History x 5 plus a Make_Content_Mil
  guess -- 15 and 10 want for an effect worth 40 gold and 20 shields a
  turn at twenty cities. The audit ranked this the cheapest structural
  win on the list and it is: the quantity is a plain sum over the
  player's own cities.

  What is summed is the bill the empire is ACTUALLY paying, not the
  allowance the effect grants. The server applies free upkeep by
  skipping the first N supported units in each output (city_support()),
  so units beyond the allowance carry the whole charge and units within
  it carry none; a wonder cannot save upkeep that is not being paid.
  Per city and output the saving is therefore the paid upkeep scaled by
  the share of paying units the new allowance would cover -- exact when
  every unit's upkeep is the ruleset's usual 1, and a mean otherwise.
**************************************************************************/
static adv_want deep_upkeep_free_credit(struct city *pcity,
                                        const struct impr_type *pimprove,
                                        struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  struct universal source = {.kind = VUT_IMPROVEMENT,
                             .value = {.building = pimprove}};
  struct effect_list *plist = get_req_source_effects(&source);
  const struct req_context context = {
    .player = pplayer,
    .city = pcity,
    .building = pimprove,
  };
  int freed[O_LAST];
  bool any = FALSE;
  adv_want credit = 0;

  if (tune->horizon_upkeepfree_weight <= 0.0 || plist == NULL) {
    return 0;
  }

  memset(freed, 0, sizeof(freed));
  effect_list_iterate(plist, peffect) {
    Output_type_id otype = O_LAST;
    bool useful = TRUE;

    if (peffect->type != EFT_UNIT_UPKEEP_FREE_PER_CITY
        || peffect->value <= 0) {
      continue;
    }
    requirement_vector_iterate(&peffect->reqs, preq) {
      if (preq->source.kind == VUT_OTYPE && preq->present) {
        otype = preq->source.value.outputtype;
        continue;
      }
      if (preq->source.kind == VUT_IMPROVEMENT
          && preq->source.value.building == pimprove) {
        /* The building we are pricing -- and the whole point of this
         * credit is that its range is the PLAYER, so a City-range
         * requirement is somebody else's effect and not ours. */
        if (!preq->present || preq->range != REQ_RANGE_PLAYER) {
          useful = FALSE;
          break;
        }
        continue;
      }
      if (!is_req_active(&context, nullptr, preq, RPT_CERTAIN)) {
        useful = FALSE;
        break;
      }
    } requirement_vector_iterate_end;

    if (useful && otype >= 0 && otype < O_LAST) {
      freed[otype] += peffect->value;
      any = TRUE;
    }
  } effect_list_iterate_end;

  if (!any) {
    return 0;
  }

  city_list_iterate(pplayer->cities, ocity) {
    output_type_iterate(o) {
      int paid = 0, payers = 0;
      adv_want saved;

      if (freed[o] <= 0) {
        continue;
      }
      unit_list_iterate(ocity->units_supported, punit) {
        if (punit->upkeep[o] > 0) {
          paid += punit->upkeep[o];
          payers++;
        }
      } unit_list_iterate_end;
      if (payers <= 0) {
        continue;
      }
      saved = (adv_want) paid * MIN(freed[o], payers) / payers;
      switch (o) {
      case O_FOOD:
        credit += saved * adv->food_priority;
        break;
      case O_SHIELD:
        credit += saved * adv->shield_priority;
        break;
      case O_GOLD:
        credit += saved * adv->gold_priority;
        break;
      default:
        break;
      }
    } output_type_iterate_end;
  } city_list_iterate_end;

  if (credit <= 0) {
    return 0;
  }

  /* Per-turn saving over the planning horizon: the same dimension as
   * the gold upkeep bill the credit is netted against, and no delay to
   * amortize -- the allowance applies from the turn it exists. */
  credit *= deep_horizon_turns() * tune->horizon_upkeepfree_weight;
  deep_upkeepfree_cands++;
  deep_upkeepfree_paid += credit;

  return credit;
}

/* get_civ_score()'s own exchange rate between culture and score
 * (server/score.c:361: `+ pplayer->score.culture / 50`). Read from the
 * scoring code rather than chosen: it is the constant the benchmark
 * itself is measured with. */
#define DEEP_CULTURE_PER_SCORE 50

/* DARKFX: the culture credit's execution counters. `war` is the
 * subset paid to cities the survival model calls threatened, because
 * this is the one credit the wartime gate deliberately does not
 * refuse and that exemption has to be visible. */
static int deep_culture_cands = 0;
static int deep_culture_war = 0;
static adv_want deep_culture_paid = 0;

/**********************************************************************//**
  Credit a building for the culture it will accumulate before the bell.
  Returns the credit for pimprove in pcity, or 0.

  The Europe derby's forensics are the evidence: culture/50 is 29.5% of
  world score at T300 and 42.5% at T600, deep prices none of its
  sources, and the decomposed cohort gap was culture -361 against noise
  in every other term. Deep held 7 Temples to stock's 28 -- not because
  it decided against them but because the horizon ladder has a credit
  for cap-lifters, growth, bundles and health and none at all for a
  Temple, so a Temple gets stock's base want (structurally zero: the
  happiness effects are #if 0'd out of advdata.c's trigger switch) plus
  History x 5.

  The arithmetic is the score's own. A building carrying `h` History
  adds h culture every turn it stands, so between completion and the
  last turn it is worth h x turns-left culture, which get_civ_score()
  pays at 1/50th of a point each -- and a point is what a citizen is
  worth, which is a price this file already computes. history_interest_
  pml is 0 in both shipped rulesets so the accumulation is linear; the
  compounding form is kept for rulesets that set it, because
  city_history_gain() does pay interest on the stock when it is not.

  What is NOT priced here: EFT_PERFORMANCE (an instantaneous culture
  term rather than an accumulating one; no shipped ruleset uses it) and
  the culture stock a conquest transfers (derby N2, and a military
  price, not a building one).
**************************************************************************/
/* Item 3's execution counters, per player pass. */
static int deep_popins_cands = 0;
static double deep_popins_paid = 0.0;
/* Item 4's, kept apart because the two streams are measured apart. */
static int deep_heal_cands = 0, deep_vet_cands = 0;
static double deep_heal_paid = 0.0, deep_vet_paid = 0.0;

/**********************************************************************//**
  THE BARRACKS' FIRST STREAM: HP THROUGHPUT (M4b item 4, audit 1c).

  Stock prices EFT_HP_REGEN at 5c + num (daieffects.c:459) -- a
  headcount WAG for a rate. The ruleset states the rate exactly:
  hp_gain_coord() (common/unit.c:2216) heals base_hp x regen% per turn,
  so what the building buys is TURNS OF THE LINE, and the population
  it buys them for is the damaged units standing in this city right
  now.

  For each of them: the turns it needs to reach full at today's regen,
  minus the turns it would need with the building's regen added. Each
  turn saved is one more unit-turn of that unit's shields on the line,
  valued at SHIELD_WEIGHTING -- the codebase's own shield-to-want
  constant, so no new scale is minted here either.

  A city with no casualties is owed nothing, which is the whole point:
  stock pays for a Barracks in a city that has never seen a fight.
**************************************************************************/
static adv_want deep_heal_stream_credit(struct city *pcity,
                                        const struct impr_type *pimprove)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  int add_regen;
  double saved = 0.0;
  adv_want credit;

  if (tune->heal_stream_gain <= 0.0) {
    return 0;
  }
  add_regen = get_potential_improvement_bonus(pimprove, pcity,
                                              EFT_HP_REGEN,
                                              RPT_CERTAIN, FALSE);
  if (add_regen <= 0) {
    return 0;
  }
  {
    int have = get_city_bonus(pcity, EFT_HP_REGEN);

    if (have >= add_regen) {
      return 0;               /* Already healing at least this fast. */
    }
    unit_list_iterate(city_tile(pcity)->units, punit) {
      const struct unit_type *ut = unit_type_get(punit);
      int missing = ut->hp - punit->hp;
      int now, with, t_now, t_with;

      if (unit_owner(punit) != pplayer
          || unit_has_type_flag(punit, UTYF_CIVILIAN)
          || missing <= 0) {
        continue;
      }
      /* The ruleset's own arithmetic, both ways. */
      now = ut->hp * have / 100;
      with = ut->hp * add_regen / 100;
      if (with <= now) {
        continue;
      }
      /* Capped at the horizon rather than called infinite: a unit that
       * cannot heal at all here is not worth an unbounded credit. */
      t_now = (now > 0
               ? (missing + now - 1) / now
               : deep_horizon_turns());
      t_with = (missing + with - 1) / with;
      t_now = MIN(t_now, deep_horizon_turns());
      if (t_now <= t_with) {
        continue;
      }
      saved += (double) (t_now - t_with)
               * unit_build_shield_cost_base(punit);
    } unit_list_iterate_end;
  }

  if (saved <= 0.0) {
    return 0;
  }
  credit = (adv_want) (saved * SHIELD_WEIGHTING * tune->heal_stream_gain);
  if (credit <= 0) {
    return 0;
  }
  deep_heal_cands++;
  deep_heal_paid += credit;

  return credit;
}

/**********************************************************************//**
  THE BARRACKS' SECOND STREAM: THE VETERAN MARGIN (audit 1c).

  This is the stream stock most plainly prices as a point. A veteran
  level is a power multiplier for the unit's WHOLE LIFE and every
  combat in it; daieffects.c:479 pays amount x (3c + num) for it, a
  headcount again.

  Both factors here are the ruleset's own numbers and neither is
  invented:

  - the MARGIN is power_fact at the level the city would build at,
    over power_fact at the level it builds at today
    (utype_veteran_level()), minus one -- a fraction. A unit that is
    that fraction stronger is worth that fraction more shields, which
    is the same identity the attack and defence ratings already use.
  - the COUNT is how many units this city gets out over the planning
    horizon: deep_horizon_turns() divided by the turns the best
    defender it can build takes to build.

  Priced on the best defender because that is the unit a city with a
  Barracks in the queue is going to be making; an attacker-heavy city
  is under-credited, which is the safe direction.
**************************************************************************/
static adv_want deep_veteran_stream_credit(struct city *pcity,
                                           const struct impr_type *pimprove)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  const struct unit_type *best = NULL;
  const struct veteran_level *vl_now, *vl_new;
  struct universal tgt;
  int add_lv, have_lv, lv_now, lv_new, levels, ttb;
  double margin, per_horizon;
  adv_want credit;

  if (tune->veteran_stream_gain <= 0.0) {
    return 0;
  }
  add_lv = get_potential_improvement_bonus(pimprove, pcity,
                                           EFT_VETERAN_BUILD,
                                           RPT_CERTAIN, FALSE);
  if (add_lv <= 0) {
    return 0;
  }
  have_lv = get_city_bonus(pcity, EFT_VETERAN_BUILD);
  if (add_lv <= have_lv) {
    return 0;                   /* No margin over what we already get. */
  }

  unit_type_iterate(ut) {
    if (ut->defense_strength > 0
        && can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)
        && (best == NULL
            || ut->defense_strength > best->defense_strength)) {
      best = ut;
    }
  } unit_type_iterate_end;
  if (best == NULL) {
    return 0;
  }

  levels = utype_veteran_levels(best);
  lv_now = MIN(have_lv, levels - 1);
  lv_new = MIN(add_lv, levels - 1);
  if (lv_new <= lv_now) {
    return 0;                   /* Ruleset caps it out. */
  }
  vl_now = utype_veteran_level(best, lv_now);
  vl_new = utype_veteran_level(best, lv_new);
  if (vl_now == NULL || vl_new == NULL || vl_now->power_fact <= 0) {
    return 0;
  }
  margin = (double) vl_new->power_fact / vl_now->power_fact - 1.0;
  if (margin <= 0.0) {
    return 0;
  }

  tgt.kind = VUT_UTYPE;
  tgt.value.utype = (struct unit_type *) best;
  ttb = city_turns_to_build(pcity, &tgt, TRUE);
  if (ttb <= 0 || ttb >= FC_INFINITY) {
    return 0;
  }
  per_horizon = (double) deep_horizon_turns() / ttb;
  if (per_horizon <= 0.0) {
    return 0;
  }

  credit = (adv_want) (per_horizon * margin
                       * utype_build_shield_cost_base(best)
                       * SHIELD_WEIGHTING
                       * tune->veteran_stream_gain);
  if (credit <= 0) {
    return 0;
  }
  deep_vet_cands++;
  deep_vet_paid += credit;

  return credit;
}

/**********************************************************************//**
  WALLS AS POP-LOSS INSURANCE (M4b item 3).

  Stock prices EFT_UNIT_NO_LOSE_POP at units x 2 (daieffects.c:456): a
  WAG over a headcount that never asks whether the city is under
  attack, so a walled capital in the interior and a walled border city
  with an army outside it are worth the same.

  The mechanic is exact and CONSEQ verified both halves of it. Capture
  costs the city one size point, flat and once
  (unit_conquer_city(), server/citytools.c:2141) -- and walls do NOT
  insure that. But every SUCCESSFUL attack costs another citizen under
  `killcitizen` plus UCF_KILLCITIZEN and absent this very effect
  (server/unithand.c:4601), and that is exactly what the effect
  removes.

  So the honest price is E[successful attacks] x what a citizen is
  worth. The first factor is not a guess: a defender dies precisely
  when an attack on it succeeds, so the chained-duel model's expected
  defender deaths ARE the expected successful attacks, which is why
  the DP now reports them. Capped at the citizens the city has left to
  lose, because the server cannot reduce it below size 1 -- past that
  the city is captured and this effect is not what was needed.

  The citizen is valued at deep_unworked_tile_value(), the same
  valuation the health credit beside it uses, so the two are directly
  comparable and this gain carries the same horizon scale that one
  does.

  NOT a double count of the walls' Defend_Bonus: that bonus is already
  inside the odds, because the survival read prices every duel through
  the tile-aware get_total_defense_power() seam. This is the
  ADDITIONAL stream on top of it. And a city that already has the
  effect from another source (civ2civ3 gives it to the Great Wall too,
  effects.ruleset:3123) is owed nothing.

  `hits` is computed once per city by the caller: it costs a DP run,
  and running one per improvement would be sixty per city per turn.
**************************************************************************/
static adv_want deep_pop_insurance_credit(struct city *pcity,
                                          const struct impr_type *pimprove,
                                          struct adv_data *adv,
                                          double hits)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double losable, insured;
  adv_want credit;

  if (tune->pop_insurance_gain <= 0.0 || hits <= 0.0) {
    return 0;
  }
  if (get_city_bonus(pcity, EFT_UNIT_NO_LOSE_POP) > 0) {
    return 0;                   /* Already insured; nothing to buy. */
  }
  if (get_potential_improvement_bonus(pimprove, pcity,
                                      EFT_UNIT_NO_LOSE_POP,
                                      RPT_CERTAIN, FALSE) <= 0) {
    return 0;
  }

  /* The server reduces the city by one per successful attack and
   * cannot take it below 1; the step past that is the capture, which
   * this effect does not insure. */
  losable = (double) city_size_get(pcity) - 1.0;
  if (losable <= 0.0) {
    return 0;
  }
  insured = MIN(hits, losable);

  credit = (adv_want) (insured
                       * deep_unworked_tile_value(pcity, adv, 3, NULL)
                       * tune->pop_insurance_gain);
  if (credit <= 0) {
    return 0;
  }

  deep_popins_cands++;
  deep_popins_paid += credit;

  return credit;
}

static adv_want deep_culture_credit(struct city *pcity,
                                    const struct impr_type *pimprove,
                                    struct adv_data *adv)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int h, left, hor;
  double gained, pml;
  adv_want credit;

  if (tune->horizon_culture_weight <= 0.0) {
    return 0;
  }
  h = get_potential_improvement_bonus(pimprove, pcity, EFT_HISTORY,
                                      RPT_CERTAIN, FALSE)
      + get_potential_improvement_bonus(pimprove, pcity,
                                        EFT_NATION_HISTORY,
                                        RPT_CERTAIN, FALSE);
  if (h <= 0) {
    return 0;
  }
  /* city_history_gain() scales the flow by EFT_CULTURE_PCT. */
  gained = (double) h * (100 + get_city_bonus(pcity, EFT_CULTURE_PCT))
           / 100.0;

  /* The horizon is the bell, not MORT: culture does not decay and the
   * score is read once, at the end. This reads game state the endgame
   * clamp also reads, and deliberately does so with endgame_horizon
   * off -- the clamp is about a 24-turn planning window being too
   * long, and this term's window is the whole game by construction.
   * With no bell set at all there is no scored end, so the credit
   * falls back to the same MORT every other credit plans over rather
   * than inventing an infinite one. */
  left = deep_turns_left();
  hor = (left >= DEEP_NO_END_TURN) ? MORT : left;
  if (hor <= 0) {
    return 0;
  }

  pml = (double) game.info.history_interest_pml / 1000.0;
  if (pml > 0.0) {
    /* Future value of a constant flow compounding at pml per turn --
     * city_history_gain() pays interest on the accumulated stock. */
    gained *= (pow(1.0 + pml, hor) - 1.0) / pml;
  } else {
    gained *= hor;
  }

  /* Culture into score into the price of the citizen that scores the
   * same single point. */
  credit = gained / DEEP_CULTURE_PER_SCORE
           * deep_unworked_tile_value(pcity, adv, 3, NULL)
           * tune->horizon_culture_weight;
  if (credit <= 0) {
    return 0;
  }

  deep_culture_cands++;
  deep_culture_paid += credit;

  return credit;
}

/* One improvement of one city that deep added credit to during the
 * current build pass, and the value it left in stock's array. */
struct deep_credit_entry {
  int city_id;
  Impr_type_id id;
  adv_want add;
  adv_want post;
};

static struct deep_credit_entry *deep_credit_ledger = NULL;
static int deep_credit_n = 0;
static int deep_credit_cap = 0;

/**********************************************************************//**
  Remember one credit so deep_revert_building_wants() can take it back
  out of stock's array when the decisions it was computed for have been
  made.
**************************************************************************/
static void deep_credit_record(const struct city *pcity, Impr_type_id id,
                               adv_want add, adv_want post)
{
  if (deep_credit_n >= deep_credit_cap) {
    deep_credit_cap = MAX(256, deep_credit_cap * 2);
    deep_credit_ledger = fc_realloc(deep_credit_ledger,
                                    deep_credit_cap
                                    * sizeof(*deep_credit_ledger));
  }
  deep_credit_ledger[deep_credit_n++] = (struct deep_credit_entry) {
    .city_id = pcity->id, .id = id, .add = add, .post = post,
  };
}

/**********************************************************************//**
  Take deep's credits back out of stock's building_want[] array, once
  every decision that reads it this turn has been made.

  Stock recalculates building_want[] on its own schedule -- every
  fc_rand(5)+5 turns per city (daicity.c) -- and leaves the array alone
  in between, which is correct for a want that stock recomputes from
  scratch when it does recompute. Deep's credit is computed EVERY turn,
  so adding it into an array nobody clears delivered the intended
  credit multiplied by the turns since that city's last recalc: 1x to
  9x, mean ~4.5x, phase-randomised per city, and clipped once the sum
  hit DAI_WANT_DOMESTIC_MAX. That is an unpriced multiplier on every
  horizon credit, an inflated within-arm variance in every credit A/B
  this project has run, and a simpler explanation than "the weight is
  too hot" for the happiness column's 10x overshoot.

  The alternatives were to credit only on stock's recalc turns, or to
  subtract deep's previous contribution before adding the new one.
  Both leave a stale credit sitting in the array between passes: the
  first freezes a credit for up to nine turns after the city stopped
  earning it (the wartime gate would be defeated by its own arrears),
  and the second still exports deep's numbers to any reader outside the
  build pass. Applying the credit for exactly the span of the decisions
  it exists to influence has neither problem and needs no cross-turn
  state: stock's array is stock's own again the moment the pass ends,
  every turn, whatever the recalc phase.

  Only a value deep itself left behind is removed. If anything else
  wrote to that improvement in between -- the wall hack in the danger
  pass is the one that can -- the entry is counted stale and left
  alone, which is a one-turn over-credit rather than a corrupted want.
**************************************************************************/
void deep_revert_building_wants(struct player *pplayer)
{
  int taken = 0, stale = 0;
  int i;

  for (i = deep_credit_n - 1; i >= 0; i--) {
    const struct deep_credit_entry *e = &deep_credit_ledger[i];
    struct city *pcity = game_city_by_number(e->city_id);

    if (pcity == NULL || pcity->server.adv == NULL
        || pcity->server.adv->building_want[e->id] != e->post) {
      stale++;
      continue;
    }
    pcity->server.adv->building_want[e->id] -= e->add;
    taken++;
  }

  if (deep_credit_n > 0) {
    log_base(deep_metric_log_level(),
             "deep-credit-revert turn=%d player=\"%s\" entries=%d"
             " taken=%d stale=%d",
             game.info.turn, player_name(pplayer), deep_credit_n,
             taken, stale);
  }
  deep_credit_n = 0;
}

/* The score mode's execution census for the current build pass:
 * improvement wants deflated because the bell arrives first, and great
 * wonders paid the completion premium. */
static int deep_endgame_refused = 0;
static int deep_endgame_paid = 0;

/**********************************************************************//**
  The endgame score mode, on the improvement half of the auction:
  inside the final `endgame_window` turns, deflate what cannot be
  finished and pay for the great wonder that can.

  Applied to stock's own building_want[] through the same credit ledger
  the horizon credits use, which buys three things. It competes INSIDE
  the domestic advisor's auction rather than against its winner -- the
  gap R3 identified, where a multiplier on the winner can only change
  whether domestic beats military and never which improvement domestic
  picks. It is reverted at the end of the pass like every other deep
  credit, so nothing outside the build decision ever sees it. And it is
  a multiplier on an existing want rather than a new want, which is
  RC4's rule.

  The threat gate above deliberately does not apply. A city about to
  fall has no endgame, but the ordinary reason to skip the growth
  ladder in wartime -- growth is a peacetime investment -- says nothing
  about whether a half-built Colosseum is worth finishing, and the
  deflator's whole job is to stop paying for things that will not
  arrive. The emergency tier is a UNIT-side concept and is handled at
  the merge seam, not here.
**************************************************************************/
static void deep_endgame_bias_buildings(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (!deep_in_endgame_window()) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    if (pcity->server.adv == NULL) {
      continue;
    }

    improvement_iterate(pimprove) {
      Impr_type_id id = improvement_number(pimprove);
      adv_want want = pcity->server.adv->building_want[id];
      struct universal target = {.kind = VUT_IMPROVEMENT,
                                 .value = {.building = pimprove}};
      double mult;
      adv_want add;

      if (want <= 0
          || !can_city_build_improvement_now(pcity, pimprove,
                                             RPT_CERTAIN)) {
        continue;
      }

      if (!deep_finishes_in_time(pcity, &target)) {
        mult = tune->endgame_unfinished_gain;
      } else if (is_great_wonder(pimprove)) {
        mult = tune->endgame_wonder_gain;
      } else {
        continue;
      }

      if (mult == 1.0) {
        continue;
      }

      add = want * (mult - 1.0);
      pcity->server.adv->building_want[id] += add;
      deep_credit_record(pcity, id, add,
                         pcity->server.adv->building_want[id]);
      if (add < 0) {
        deep_endgame_refused++;
      } else {
        deep_endgame_paid++;
      }

      log_base(LOG_DEBUG,
               "deep-endgame-impr city=%d impr=\"%s\" want=%.1f"
               " mult=%.2f", pcity->id, improvement_rule_name(pimprove),
               (double) want, mult);
    } improvement_iterate_end;
  } city_list_iterate_end;
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
  /* Gate 3 (W34 A.2): what growth is worth to THIS player, as
   * (econ/mil)^stance_growth_gain -- exactly 1.0 at identity axes.
   * These credits are the growth ladder, and the shields they bid for
   * are the same ones barracks and walls want, so this is the gate
   * where "build up rather than out, or arm" is actually decided. */
  double gmult = deep_stance_growth_mult(pplayer);

  if (adv == NULL) {
    return;
  }

  deep_horizon_clamped = 0;
  deep_horizon_missed = 0;
  deep_endgame_refused = 0;
  deep_endgame_paid = 0;
  deep_govcenter_cities = 0;
  deep_govcenter_paid = 0;
  deep_upkeepfree_cands = 0;
  deep_upkeepfree_paid = 0;
  deep_culture_cands = 0;
  deep_culture_war = 0;
  deep_culture_paid = 0;
  deep_popins_cands = 0;
  deep_popins_paid = 0.0;
  deep_heal_cands = 0;
  deep_heal_paid = 0.0;
  deep_vet_cands = 0;
  deep_vet_paid = 0.0;

  city_list_iterate(pplayer->cities, pcity) {
    adv_want cap_credit;
    bool threatened;
    /* Item 3's input, taken ONCE per city: expected successful enemy
     * attacks over the survival horizon. One DP run, not one per
     * improvement. Zero without the knob, so the off arm does no work
     * at all. */
    double pop_hits = 0.0;
    /* What this city's credit actually DELIVERS this turn, as opposed
     * to what it intends. The two are not the same number: stock
     * clears building_want[] only on its own recalc schedule
     * (daicity.c, fc_rand(5)+5 turns), so a credit added every turn
     * lands on top of the credits of every turn since the last
     * recalc. `pre` is what deep found in the array before it added
     * anything, summed over the improvements it credits; on a stock
     * recalc turn that is stock's own fresh want, and on every other
     * turn it carries deep's own past contributions. A flat `pre`
     * across a recalc window is the fix; a rising one is the
     * sawtooth. */
    adv_want tr_pre = 0, tr_add = 0, tr_post = 0;
    adv_want tr_toppre = 0, tr_topadd = 0;
    /* The health term's own instrument (knob-coverage 4e listed
     * horizon_health_weight as having none): summed over the city's
     * improvements, BEFORE the stance multiplier and the upkeep bill,
     * so it reports the term rather than the ladder's total. */
    adv_want tr_health = 0;
    const struct impr_type *tr_top = NULL;
    int tr_n = 0;

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
      /* The SAME verdict the citizen arrangement read at the top of
       * this turn (deepcm.c), with this layer's own threshold: one
       * probability, two prices. */
      double surv = deep_city_survival_verdict(&(wld.map), pcity,
                                               tune->survival_horizon,
                                               &wave);

      threatened = wave > 0 && surv < tune->survival_target;
    } else {
      threatened = def_ai_city_data(pcity, ait)->urgency > 0
                   || def_ai_city_data(pcity, ait)->grave_danger > 0;
    }
    if (tune->pop_insurance_gain > 0.0 && tune->survival_target > 0.0) {
      int wave = 0;

      /* The cached verdict answers "is anyone coming" for free; only
       * then is the fuller read worth its DP run. */
      (void) deep_city_survival_verdict(&(wld.map), pcity,
                                        tune->survival_horizon, &wave);
      if (wave > 0) {
        struct deep_defence_read dr;

        deep_city_defence_read(&(wld.map), pcity, tune->survival_horizon,
                               NULL, 0, &dr);
        pop_hits = dr.hits;
      }
    }

    cap_credit = deep_capacity_credit(pcity, adv);
    if (threatened && cap_credit <= 0
        && tune->horizon_culture_weight <= 0.0
        /* And the pop insurance, for the sharper reason: a THREATENED
         * city is the only city that has any expected successful
         * attacks at all, so gating the insurance on peace would gate
         * it on having no population. */
        && !(tune->pop_insurance_gain > 0.0 && pop_hits > 0.0)
        /* Item 4 the same: a Barracks is worth most to a city that is
         * taking casualties and building replacements. */
        && tune->heal_stream_gain <= 0.0
        && tune->veteran_stream_gain <= 0.0) {
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
        /* Gate 3 deliberately does NOT reach this branch: the credit
         * that leaves the build queue and lands in tech_want is tech
         * DIRECTION, which is W34 A.3's gate and E34.4's to build.
         * Scaling it here would mean two gates on one quantity, and
         * "do not let the axis compensate for a scale gap" (A.3). */
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
      if (!threatened) {
        add += deep_bundle_credit(pcity, pimprove, adv);
        add += deep_content_credit(pcity, pimprove, adv);
        add += deep_multiplier_credit(pcity, pimprove, adv);
        /* Growth accelerator: peace-time investment like the other two.
         * The structural cap-lifter above keeps its wartime exemption
         * because a size GATE never reopens on its own; a Granary is
         * only ever an optimisation. */
        add += deep_growth_credit(pcity, pimprove, adv);
        /* DARKFX: what a government centre here saves every OTHER
         * city. Peace-time investment like the rest of the ladder. */
        add += deep_gov_center_credit(pcity, pimprove, adv);
        /* DARKFX: what a Player-range free-upkeep effect saves every
         * city, rather than what its History value guesses. */
        add += deep_upkeep_free_credit(pcity, pimprove, adv);
      }

      {
        /* DARKFX: culture accrues to a city whether or not it is at
         * war, and it is the one term the wartime gate above must not
         * refuse -- the derby's diagnosis is precisely that deep's
         * threatened cities held no Temples for six hundred turns
         * while the metric they were losing on was culture. The
         * exemption is counted rather than assumed. */
        adv_want ccred = deep_culture_credit(pcity, pimprove, adv);

        if (ccred > 0) {
          add += ccred;
          if (threatened) {
            deep_culture_war++;
          }
        }
      }

      /* M4b item 3, and NOT gated on !threatened for the same reason
       * culture is not: a wall's insurance value is zero in a city
       * nobody is attacking and maximal in one that is. */
      add += deep_pop_insurance_credit(pcity, pimprove, adv, pop_hits);

      /* M4b item 4: the Barracks' two real streams, likewise not
       * gated on !threatened -- healing and veterancy are war goods,
       * and stock's own two WAGs for them are headcounts that do not
       * know whether there is a war on. */
      add += deep_heal_stream_credit(pcity, pimprove);
      add += deep_veteran_stream_credit(pcity, pimprove);

      if (!threatened && game.info.illness_on
          && city_size_get(pcity) + 1 >= game.info.illness_min_size) {
        int health = get_potential_improvement_bonus(pimprove, pcity,
                                                     EFT_HEALTH_PCT,
                                                     RPT_CERTAIN, FALSE);

        if (health > 0) {
          /* Plague insurance is worth having BEFORE the size gate the
           * building's other effects wait for (civ2civ3: Aqueduct
           * cuts illness 30% from size 5; Size_Adj matters at 8).
           *
           * In the SAME currency as every other credit here: citizens
           * over the horizon, valued at what a citizen earns from the
           * best unworked tile. This used to add raw EFT_HEALTH_PCT
           * PERCENT POINTS times a weight -- a third unit beside the
           * tile-sums and the gold bill, so `horizon_health_weight`
           * could not be compared with its neighbours and a 30%
           * Aqueduct scored 30 whatever the city's actual plague risk
           * was, including in a city whose risk was zero (economy
           * audit 3.11; D2 fixed the upkeep dimension and left this
           * one).
           *
           * The conversion is the ruleset's own arithmetic:
           * city_illness_calc() reports base illness in tenths of a
           * percent per turn BEFORE health effects, a strike costs
           * exactly one citizen (city_illness_strike(),
           * citytools.c:2984), and the building removes `health`
           * percent of that risk. So expected citizens saved over the
           * planning horizon = ill_base/1000 * health/100 * MORT, and
           * the value of a citizen is the one this file already pays
           * everywhere else. No amortize(): the risk is running now,
           * not waiting on a growth step.
           *
           * The knob's SCALE therefore changes with its dimension; it
           * is a PLACEHOLDER and owes the tuning pile a value. There
           * is no exact conversion factor to divide by the way the
           * upkeep fix had MORT, so this commit does not pretend the
           * charge lands unchanged. */
          int ill_base = 0;

          (void) city_illness_calc(pcity, &ill_base, NULL, NULL, NULL);
          if (ill_base > 0) {
            double saved = (double) ill_base / 1000.0
                           * MIN(health, 100) / 100.0
                           * deep_horizon_turns();
            adv_want hcred
              = saved * deep_unworked_tile_value(pcity, adv, 3, NULL)
                * deep_tuning_get()->horizon_health_weight;

            add += hcred;
            tr_health += hcred;
          }
        }
      }

      if (add > 0 && gmult != 1.0) {
        adv_want scaled = add * gmult;

        /* A THREATENED city's only surviving credit is the structural
         * cap-lifter, and that one is a floor rather than a taste: a
         * city at the ruleset's size gate stays there for the rest of
         * the game, and in a permanent-war world the gate never
         * reopens. A military weight may decline the growth ladder;
         * it may not talk the empire into a permanent size cap. An
         * economic weight still raises it. */
        add = threatened ? MAX(add, scaled) : scaled;
        deep_plr_data(pplayer)->stance_growth_gated++;
      }
      if (add > 0) {
        /* The credit imagines the building's future yields, but the
         * ruleset may charge gold every turn to keep it (civ2civ3:
         * every building; head-to-head games showed deep bankrupting
         * itself on exactly that bill, gold ratio 0.13-0.55 vs
         * stock). Charge the ruleset's own upkeep figure against the
         * credit -- never below zero, so stock's base want keeps its
         * pricing.
         *
         * Over the SAME horizon the credit is summed over. Every
         * credit above counts a per-turn value across MORT turns
         * (steps = MORT/ttg) and discounts once for the delay; the
         * bill is a per-turn cost across the same MORT turns, and
         * subtracting a single turn of it from a MORT-horizon value
         * was a dimensional error that made the ruleset's bill
         * roughly twenty-four times too cheap. upkeep_gold_weight's
         * default and registered range are divided by MORT in the
         * same commit, so the charge LANDS unchanged and this fix
         * does not smuggle a 24x tightening into the round's A/B;
         * the knob now means "gold per turn against the horizon" and
         * owes the tuning pile a value. */
        add -= city_improvement_upkeep(pcity, pimprove)
               * deep_horizon_turns()
               * deep_tuning_get()->upkeep_gold_weight;
        add = MAX(add, 0);
      }
      if (add > 0 && deep_tuning_get()->expiry_building_gain > 0.0) {
        /* EXPIRY-AWARE AMORTIZATION (CONSEQ item 4, human 2026-08-05:
         * "some units and buildings expire too").
         *
         * Every credit above sums a per-turn value over a MORT-turn
         * horizon and assumes the building is there for all of it.
         * It is not, when OUR OWN research queue is about to void it:
         * the server auto-sells an obsolete improvement
         * (remove_obsolete_buildings_city()) and its effects lapse
         * before that anyway, so the shields buy a fraction of the
         * horizon and the AI currently pays for all of it. The
         * Barracks-under-Gunpowder case.
         *
         * The truncation is the share of the horizon that survives,
         * and the ETA is our own research only -- deep_tech_eta()'s
         * whole contract. -1 ("not on the way") leaves the credit
         * alone, so a building nobody is researching past keeps its
         * full window. */
        int ob = deep_impr_obsolete_eta(pplayer, pimprove);

        if (ob >= 0) {
          double horizon = MAX(1.0, deep_horizon_turns());
          double share = MIN(1.0, ob / horizon);
          double keep = 1.0 - deep_tuning_get()->expiry_building_gain
                              * (1.0 - share);

          log_base(deep_metric_log_level(),
                   "deep-expiry turn=%d player=\"%s\" city=%d"
                   " impr=\"%s\" eta=%d horizon=%.1f share=%.3f"
                   " credit=%.1f keep=%.3f",
                   game.info.turn, player_name(pplayer), pcity->id,
                   improvement_rule_name(pimprove), ob, horizon, share,
                   (double) add, keep);
          add = (adv_want) (add * MAX(keep, 0.0));
        }
      }
      if (add > 0) {
        adv_want pre = pcity->server.adv->building_want[id];

        pcity->server.adv->building_want[id] += add;
        deep_credit_record(pcity, id,
                           add, pcity->server.adv->building_want[id]);
        tr_n++;
        tr_pre += pre;
        tr_add += add;
        tr_post += pcity->server.adv->building_want[id];
        if (tr_top == NULL || add > tr_topadd) {
          tr_top = pimprove;
          tr_topadd = add;
          tr_toppre = pre;
        }
        log_base(LOG_DEBUG,
                 "deep-horizon city=%d impr=\"%s\" credit=%.1f",
                 pcity->id, improvement_rule_name(pimprove),
                 (double) add);
      }
    } improvement_iterate_end;

    if (tr_n > 0) {
      log_base(deep_metric_log_level(),
               "deep-credit turn=%d player=\"%s\" city=%d size=%d n=%d"
               " pre=%.1f add=%.1f post=%.1f top=\"%s\" toppre=%.1f"
               " topadd=%.1f health=%.2f",
               game.info.turn, player_name(pplayer), pcity->id,
               city_size_get(pcity), tr_n, (double) tr_pre,
               (double) tr_add, (double) tr_post,
               improvement_rule_name(tr_top), (double) tr_toppre,
               (double) tr_topadd, (double) tr_health);
    }
  } city_list_iterate_end;

  /* The score mode runs AFTER the horizon credits, on the array they
   * have already written to: the deflator is meant to act on the want
   * the city would really have carried into the auction, credits and
   * all, and the wonder premium on the same. */
  deep_endgame_bias_buildings(pplayer);

  /* DARKFX's first execution counter. Zero lines a game with the knob
   * off is the off-switch proof; `cands` counts the (city,
   * improvement) pairs that found a saving, which on civ2civ3 is the
   * Ecclesiastical Palace and nothing else, and on classic -- which
   * has no second gov-centre building at all -- is structurally zero. */
  if (deep_govcenter_cities > 0) {
    log_base(deep_metric_log_level(),
             "deep-govcenter turn=%d player=\"%s\" cands=%d credit=%.1f",
             game.info.turn, player_name(pplayer),
             deep_govcenter_cities, (double) deep_govcenter_paid);
  }

  if (deep_culture_cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-culture turn=%d player=\"%s\" cands=%d war=%d"
             " left=%d credit=%.1f",
             game.info.turn, player_name(pplayer), deep_culture_cands,
             deep_culture_war, deep_turns_left(),
             (double) deep_culture_paid);
  }

  /* Item 3's counter. Zero lines a game at the default gain is half
   * the off-switch proof; the other half is that no DP run happens. */
  if (deep_popins_cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-popins turn=%d player=\"%s\" cands=%d credit=%.1f",
             game.info.turn, player_name(pplayer), deep_popins_cands,
             deep_popins_paid);
  }

  /* Item 4's two streams counted APART, because they are priced
   * apart and a session that cannot tell which one moved has learnt
   * nothing. */
  if (deep_heal_cands > 0 || deep_vet_cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-barracks turn=%d player=\"%s\" heal=%d healcr=%.1f"
             " vet=%d vetcr=%.1f",
             game.info.turn, player_name(pplayer), deep_heal_cands,
             deep_heal_paid, deep_vet_cands, deep_vet_paid);
  }

  if (deep_upkeepfree_cands > 0) {
    log_base(deep_metric_log_level(),
             "deep-upkeepfree turn=%d player=\"%s\" cands=%d credit=%.1f",
             game.info.turn, player_name(pplayer),
             deep_upkeepfree_cands, (double) deep_upkeepfree_paid);
  }

  /* The clamp's execution counter, one line per player per turn, and
   * the reason the "inert before the window" claim is checkable rather
   * than argued: `clamps` is zero on every turn more than MORT from
   * the end, and zero on every turn at all with the knob off. The two
   * mechanisms are counted apart because they are measured apart. */
  if (deep_horizon_clamped > 0 || deep_horizon_missed > 0
      || deep_endgame_refused > 0 || deep_endgame_paid > 0) {
    int left = deep_turns_left();

    log_base(deep_metric_log_level(),
             "deep-endgame turn=%d player=\"%s\" left=%d horizon=%d"
             " clamps=%d missed=%d refused=%d paid=%d",
             game.info.turn, player_name(pplayer), left,
             MIN(MORT, left), deep_horizon_clamped,
             deep_horizon_missed, deep_endgame_refused,
             deep_endgame_paid);
  }
}
