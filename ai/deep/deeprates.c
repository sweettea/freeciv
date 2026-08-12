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

/* common */
#include "city.h"
#include "effects.h"
#include "game.h"
#include "government.h"
#include "player.h"
#include "research.h"
#include "tech.h"

/* ai */
#include "handicaps.h"

/* server */
#include "citytools.h"
#include "plrhand.h"

/* server/advisors */
#include "advdata.h"
#include "advtools.h"

/* ai/default */
#include "daidata.h"
#include "daihand.h"
#include "daiplayer.h"
#include "daitools.h"

#include "deepai.h"
#include "deephorizon.h"
#include "deeptuning.h"

#include "deeprates.h"

/* Tax, science and luxury rates.
 *
 * Stock (dai_manage_taxes, daihand.c) treats the empire as one city
 * and walks a SOLVENCY ladder: the minimum tax that balances the
 * books, the minimum science that avoids losing a tech, a celebration
 * test, then a fixed preference order for the remainder. It is a good
 * ladder and it prices nothing: no tech, anywhere, is ever given a
 * value, so science is whatever the treasury did not need. Deep then
 * READS the resulting split when valuing a Marketplace or a Library
 * (deephorizon.c) -- a solvency artefact read as a statement of
 * strategic value.
 *
 * Deep's policy keeps stock's two hard parts and replaces the ladder
 * in between:
 *
 *  - SOLVENCY stays a constraint, not a preference. A triple is
 *    feasible if the projected balance is non-negative, or if the
 *    treasury can absorb the deficit and still hold stock's own buy
 *    float (dai_gold_reserve). Deep has a recorded history of
 *    bankrupting itself; the guard is not the part to relax.
 *  - CELEBRATION stays stock's. Rapture is a rates + city-manager +
 *    government coupling and gets its own session; until then, when
 *    stock decides to celebrate deep does not overrule it, because
 *    stock has already spent a full CM query per city establishing
 *    that the luxury rate buys something this model cannot see (a
 *    THRESHOLD payoff, where the model below saturates).
 *
 * Between those, the split is chosen by comparing what each point
 * BUYS, in deep's existing want currency and with deep's existing
 * prices:
 *
 *  - a bulb is worth adv->science_priority per turn, DISCOUNTED by how
 *    far the current research still has to run. The discount is the
 *    part stock never applies: bulbs bought twenty turns from the next
 *    advance buy a twenty-turn-away advance.
 *  - a gold is worth adv->gold_priority per turn.
 *  - a luxury point buys happiness, and happiness hands citizens back
 *    to the tiles: deep_pacification_load() counts the citizens a city
 *    currently spends on order and deep_unworked_tile_value() says
 *    what one of them earns -- the same pair deep_content_credit()
 *    prices a Temple with. The term SATURATES at the load, because an
 *    already-content empire gets nothing more from luxury. That
 *    saturation is also where celebration would take over, which is a
 *    threshold and not a slope, and is why the celebration branch is
 *    left to stock.
 *
 * WHY THOSE PRICES, and the one that was tried first. All three terms
 * have to be in ONE currency or the comparison is theatre. The
 * advisor's *_priority family is that currency: deep_unworked_tile_
 * value() -- and therefore per_citizen, and therefore every horizon
 * credit -- is literally a sum of tile outputs times those very
 * numbers, so pricing gold and science with them makes the three
 * commensurable by construction. Their own source comment says "They
 * are all WAGs" (advdata.c:508) and that is a real weakness, recorded
 * rather than hidden.
 *
 * The first cut priced gold at deep's own upkeep_gold_weight instead
 * and left science on stock's tech_want[]. It measured a degenerate
 * all-luxury split in every game: upkeep_gold_weight values a gold at
 * 3 want where a freed citizen is worth 1300, a 170x disagreement
 * about what one trade point is worth. tech_want[] has the same
 * problem from the other side -- it is the ONLY place in the codebase
 * a tech is priced, but it lives in stock's want currency, and the
 * conversion to deep's is W01's `want_t`, which was declared and
 * never built. So tech_want gates the override (no price, no opinion)
 * and is logged every turn, and does not scale the science term.
 * Naming that missing conversion is the finding; guessing it would be
 * rule 7.
 */

/**********************************************************************//**
  Output per rate point: what the empire really produced last turn,
  divided by the rate that produced it. Falls back to the trade share
  when there is no measurement to divide -- a rate that was zero, or an
  output that came back zero anyway.
**************************************************************************/
static double deep_rate_slope(double measured, int rate, int trade)
{
  if (rate > 0 && measured > 0.0) {
    return measured / rate;
  }
  return trade / 100.0;
}

/**********************************************************************//**
  Score one legal triple.
**************************************************************************/
static double deep_rate_value(const struct deep_rate_model *m,
                              int sci, int tax, int lux,
                              double *v_sci, double *v_gold,
                              double *v_lux, bool *feasible)
{
  double bulbs = m->bulb_slope * sci;
  double gold = m->gold_slope * tax - m->expenses;

  /* Science: the bulb stream at the advisor's price for it, discounted
   * by how long the current research still has to run. amortize() is
   * the same discount every other deep credit uses. With zero bulbs
   * the research does not advance at all, so the term is zero rather
   * than a deeply discounted something. */
  if (bulbs <= 0.0) {
    *v_sci = 0.0;
  } else {
    int wait = (m->bulbs_needed > 0.0
                ? (int) ceil(m->bulbs_needed / bulbs) : 0);

    *v_sci = amortize(bulbs * m->sci_priority, wait) * MORT * m->sci_gain;
  }

  /* Gold: the net stream at the advisor's price for it, over the same
   * MORT horizon. Expenses are the same in every triple, so they shift
   * all scores equally and are kept only so the logged number means
   * "net gold". */
  *v_gold = gold * m->gold_priority * MORT;

  /* Luxury: read off the per-city table, which already saturated each
   * city at its own entertainer count. */
  *v_lux = m->lux_by_rate[lux / 10] * MORT * m->lux_gain;

  *feasible = (gold >= 0.0 || m->treasury + gold >= m->reserve);
  if (*feasible && m->tech_upkeep > 0.0) {
    /* Same shape as the gold test: a shortfall is allowed only while
     * the bank covers it. Losing a tech to buy a luxury point is not
     * a trade this comparison is allowed to make. */
    double bulb_balance = bulbs - m->tech_upkeep;

    *feasible = (bulb_balance >= 0.0 || m->bulb_bank + bulb_balance >= 0.0);
  }

  return *v_sci + *v_gold + *v_lux;
}

/**********************************************************************//**
  Pick the split. Enumerates every legal triple at the ruleset's rate
  granularity -- 66 of them at step 10 -- and takes the best feasible
  one, falling back to the best of all when nothing is solvent (which
  is the situation stock's "risk of tech loss" branch also handles by
  maximising tax).
**************************************************************************/
struct deep_rate_pick deep_rates_pick(const struct deep_rate_model *m)
{
  struct deep_rate_pick best = {.sci = -1};
  struct deep_rate_pick bestany = {.sci = -1};
  int step = MAX(1, m->step);
  int sci, tax, lux;

  for (sci = 0; sci <= m->maxrate; sci += step) {
    for (tax = 0; tax + sci <= 100 && tax <= m->maxrate; tax += step) {
      struct deep_rate_pick c;
      bool feasible;

      lux = 100 - sci - tax;
      if (lux < 0 || lux > m->maxrate || lux % step != 0) {
        continue;
      }
      c.sci = sci;
      c.tax = tax;
      c.lux = lux;
      c.value = deep_rate_value(m, sci, tax, lux, &c.v_sci, &c.v_gold,
                                &c.v_lux, &feasible);
      best.considered++;
      bestany.considered++;
      if (feasible) {
        best.feasible++;
        bestany.feasible++;
      }
      if (bestany.sci < 0 || c.value > bestany.value) {
        int keep_considered = bestany.considered;
        int keep_feasible = bestany.feasible;

        bestany = c;
        bestany.considered = keep_considered;
        bestany.feasible = keep_feasible;
      }
      if (feasible && (best.sci < 0 || c.value > best.value)) {
        int keep_considered = best.considered;
        int keep_feasible = best.feasible;

        best = c;
        best.considered = keep_considered;
        best.feasible = keep_feasible;
      }
    }
  }

  if (best.sci < 0) {
    best = bestany;
  }
  return best;
}

/**********************************************************************//**
  What one step of research is worth to this empire, read out of the
  only place a tech is priced at all: stock's tech_want[] array.

  dai_select_tech() (daitech.c, static) chooses its goal by spreading
  each tech's want over the techs required to reach it, divided by the
  step count. The same convention is used here: a tech we are one step
  from contributes its whole want, one three steps away contributes a
  third, and the current research is credited with every want whose
  path runs through it. That is what the bulbs being bought actually
  advance.

  Must be called BEFORE dai_clear_tech_wants() -- the array is cleared
  at the top of every last_activities pass and refilled by the city
  loop that runs AFTER the rates are set, so the live array at rate
  time is empty and last turn's is the honest one. A one-turn-stale
  read of an empire-level appetite is not a problem; reading zero
  would be.
**************************************************************************/
void deep_rates_snapshot_tech(struct ai_type *ait, struct player *pplayer)
{
  const struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);
  const struct research *presearch = research_get(pplayer);
  struct deep_plr *dp = deep_plr_data(pplayer);
  Tech_type_id researching;
  double want = 0.0;

  dp->rates_tech_want = 0.0;
  if (plr_data == NULL || presearch == NULL) {
    return;
  }
  researching = presearch->researching;
  if (!valid_advance_by_number(researching)) {
    /* Future tech or nothing being researched: there is no step to
     * price, and the science term will be zero. */
    return;
  }

  advance_index_iterate(A_FIRST, i) {
    int steps;

    if (plr_data->tech_want[i] <= 0
        || research_invention_state(presearch, i) == TECH_KNOWN
        || !research_goal_tech_req(presearch, i, researching)) {
      continue;
    }
    steps = research_goal_unknown_techs(presearch, i);
    want += (double) plr_data->tech_want[i] / MAX(1, steps);
  } advance_index_iterate_end;

  /* A tech we are researching FOR ITSELF is not a prerequisite of
   * itself as far as research_goal_tech_req() is concerned in every
   * ruleset shape, so add its own want explicitly if it was not
   * already counted. */
  if (plr_data->tech_want[researching] > 0
      && !research_goal_tech_req(presearch, researching, researching)) {
    want += plr_data->tech_want[researching];
  }

  dp->rates_tech_want = want;
}

/**********************************************************************//**
  The rate ceiling this player is bound by right now: the ruleset's
  EFT_MAX_RATES under the government currently SET, clipped the way
  stock clips it. Split out of the model builder so the government
  valuation can ask the same question of a candidate government and
  get the same answer -- one ceiling, one definition.
**************************************************************************/
int deep_rates_max_rate(const struct player *pplayer)
{
  int maxrate = (has_handicap(pplayer, H_RATES)
                 ? get_player_bonus(pplayer, EFT_MAX_RATES) : 100);

  /* Stock's own clip: no government in any shipped ruleset allows
   * less than a third, and a maxrate below it makes the enumeration
   * empty. */
  return CLIP(34, maxrate, 100);
}

/**********************************************************************//**
  Gather the live empire into a deep_rate_model.

  Returns FALSE when the model cannot be built honestly -- no cities,
  no trade, or no price for research yet (turn one, before any city
  has expressed a tech want). Declining is the right answer there:
  stock's ladder is a perfectly good default and deep has nothing to
  say.
**************************************************************************/
bool deep_rates_model(struct ai_type *ait, struct player *pplayer,
                      struct deep_rate_model *m)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct research *presearch = research_get(pplayer);
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct deep_plr *dp = deep_plr_data(pplayer);
  int trade = 0, expenses = 0, income = 0;
  int lux_now = 0;
  double load_total = 0.0, worth_total = 0.0;
  int s0 = pplayer->economic.science;
  int t0 = pplayer->economic.tax;
  int l0 = pplayer->economic.luxury;
  /* The rates that PRODUCED last turn's outputs: the ones deep left in
   * place, not the ones stock has just re-chosen this turn. */
  int lux_rate = dp->rates_applied ? dp->rates_last_lux : l0;

  if (adv == NULL || presearch == NULL || city_list_size(pplayer->cities) < 1) {
    return FALSE;
  }

  dai_calc_data(pplayer, &trade, &expenses, &income);
  if (trade <= 0) {
    return FALSE;
  }

  memset(m->lux_by_rate, 0, sizeof(m->lux_by_rate));
  city_list_iterate(pplayer->cities, pcity) {
    /* Entertainers, NOT the whole pacification load. An unhappy
     * citizen is still on a tile and still producing; only the
     * specialists the arrangement made to keep order are output that
     * luxury could hand back. Counting the unhappy ones too priced a
     * citizen that was already working as if it were idle, and the
     * first measurement of that put every empire on 0 science and 60
     * luxury from the moment any city went unhappy. */
    int load = deep_entertainer_load(pcity);
    int cur = pcity->prod[O_LUXURY];
    double steps = (double) cur / MAX(1, game.info.happy_cost);
    /* The citizens luxury could still hand back to the tiles, plus the
     * ones the luxury already flowing is holding: the model has to
     * value the CURRENT luxury rate too, or it would price its own
     * status quo at zero. */
    double cap = load + steps;
    double worth = deep_unworked_tile_value(pcity, adv, 3, NULL);
    double slope = deep_rate_slope(cur, lux_rate, pcity->surplus[O_TRADE]);
    int i;

    lux_now += cur;
    load_total += cap;
    worth_total += cap * worth;
    for (i = 0; i <= 10; i++) {
      double freed = slope * 10 * i / MAX(1, game.info.happy_cost);

      m->lux_by_rate[i] += MIN(freed, cap) * worth;
    }
  } city_list_iterate_end;

  m->maxrate = deep_rates_max_rate(pplayer);
  m->step = 10;

  /* Measured yields per rate point: last turn's real output divided by
   * the rate that PRODUCED it, which is the rate deep left in place at
   * the end of last turn and not the one stock has just re-chosen this
   * turn. Getting that wrong is a trap and not a rounding error: the
   * first cut divided last turn's bulbs by this turn's science rate,
   * so one turn at science 0 made the bulb slope 0, which made science
   * worthless, which set science to 0 again -- every game pinned at
   * zero research from turn 5. A rate that was zero has no measurement
   * to divide, and falls back to the trade-share estimate stock's
   * distribute() makes. */
  m->bulb_slope = deep_rate_slope(pplayer->server.bulbs_last_turn,
                                  dp->rates_applied ? dp->rates_last_sci : s0,
                                  trade);
  m->gold_slope = deep_rate_slope(income,
                                  dp->rates_applied ? dp->rates_last_tax : t0,
                                  trade);
  m->lux_slope = deep_rate_slope(lux_now, lux_rate, trade);

  m->expenses = expenses;
  m->treasury = pplayer->economic.gold;
  m->reserve = dai_gold_reserve(pplayer);

  m->tech_want = dp->rates_tech_want;
  m->bulbs_needed =
    MAX(0, research_total_bulbs_required(presearch, presearch->researching,
                                         FALSE)
           - presearch->bulbs_researched);
  m->sci_priority = adv->science_priority;
  m->sci_gain = tune->rates_science_gain;
  m->tech_upkeep = (game.info.tech_upkeep_style != TECH_UPKEEP_NONE
                    ? player_tech_upkeep(pplayer) : 0);
  m->bulb_bank = presearch->bulbs_researched;

  m->free_cap = load_total;
  m->per_citizen = (load_total > 0.0 ? worth_total / load_total : 0.0);
  m->happy_cost = MAX(1, game.info.happy_cost);
  m->lux_gain = tune->rates_luxury_gain;

  m->gold_priority = adv->gold_priority;

  return m->tech_want > 0.0;
}

/**********************************************************************//**
  Deep's rate management: run stock's, then override its choice with a
  merit comparison unless stock has a reason deep cannot see.

  Running stock FIRST rather than instead is deliberate. It keeps the
  celebration decision, the city_data->celebrate flags and adv->celebrate
  exactly as they are -- rapture is a coupled rates/CM/government
  mechanism with a threshold payoff that the saturating luxury model
  here cannot represent, and half-landing it would silently delete a
  growth mechanism from the classic cells. It also means the off switch
  is the whole feature: with rates_enabled == 0 this function is
  dai_manage_taxes and nothing else.
**************************************************************************/
static void deep_rates_remember(struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  /* Whatever is in effect when this function ends is the split that
   * will PRODUCE next turn's bulbs, gold and luxury -- which is what
   * next turn's slopes have to divide by. */
  dp->rates_last_sci = pplayer->economic.science;
  dp->rates_last_tax = pplayer->economic.tax;
  dp->rates_last_lux = pplayer->economic.luxury;
  dp->rates_applied = TRUE;
}

void deep_manage_taxes(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct adv_data *adv;
  struct deep_rate_model m;
  struct deep_rate_pick pick;
  const char *mode;
  int was_sci, was_tax, was_lux;

  dai_manage_taxes(ait, pplayer);

  if (tune->rates_enabled == 0) {
    return;
  }
  if (!game.info.changable_tax
      || government_of_player(pplayer) == game.government_during_revolution
      || !pplayer->is_alive) {
    /* The same three refusals stock makes; there is no split to set. */
    return;
  }

  adv = adv_data_get(pplayer, NULL);
  if (adv != NULL && adv->celebrate) {
    deep_plr_data(pplayer)->rates_celebrate++;
    log_base(deep_metric_log_level(),
             "deep-rates turn=%d player=\"%s\" mode=celebrate"
             " sci=%d tax=%d lux=%d",
             game.info.turn, player_name(pplayer),
             pplayer->economic.science, pplayer->economic.tax,
             pplayer->economic.luxury);
    deep_rates_remember(pplayer);
    return;
  }

  if (!deep_rates_model(ait, pplayer, &m)) {
    deep_plr_data(pplayer)->rates_unpriced++;
    log_base(deep_metric_log_level(),
             "deep-rates turn=%d player=\"%s\" mode=unpriced"
             " sci=%d tax=%d lux=%d",
             game.info.turn, player_name(pplayer),
             pplayer->economic.science, pplayer->economic.tax,
             pplayer->economic.luxury);
    deep_rates_remember(pplayer);
    return;
  }

  pick = deep_rates_pick(&m);
  was_sci = pplayer->economic.science;
  was_tax = pplayer->economic.tax;
  was_lux = pplayer->economic.luxury;

  if (pick.sci < 0) {
    mode = "empty";
  } else if (pick.sci == was_sci && pick.tax == was_tax
             && pick.lux == was_lux) {
    mode = "agree";
    deep_plr_data(pplayer)->rates_agreed++;
  } else {
    mode = "set";
    pplayer->economic.science = pick.sci;
    pplayer->economic.tax = pick.tax;
    pplayer->economic.luxury = pick.lux;
    deep_plr_data(pplayer)->rates_set++;

    /* The cities' outputs were computed under the old split, and the
     * build pass reads them immediately after this. Stock refreshes
     * for the same reason after its celebration probe. */
    city_list_iterate(pplayer->cities, pcity) {
      city_refresh_from_main_map(nmap, pcity, NULL);
    } city_list_iterate_end;
    send_player_info_c(pplayer, pplayer->connections);
  }

  log_base(deep_metric_log_level(),
           "deep-rates turn=%d player=\"%s\" mode=%s stock=%d/%d/%d"
           " deep=%d/%d/%d value=%.1f vsci=%.1f vgold=%.1f vlux=%.1f"
           " techwant=%.1f bulbs=%.2f gold=%.2f lux=%.2f cap=%.1f"
           " percit=%.1f need=%.0f treasury=%.0f reserve=%.0f"
           " feasible=%d/%d maxrate=%d",
           game.info.turn, player_name(pplayer), mode,
           was_sci, was_lux, was_tax,
           pick.sci, pick.lux, pick.tax,
           pick.value, pick.v_sci, pick.v_gold, pick.v_lux,
           m.tech_want, m.bulb_slope, m.gold_slope, m.lux_slope,
           m.free_cap, m.per_citizen, m.bulbs_needed, m.treasury,
           m.reserve, pick.feasible, pick.considered, m.maxrate);

  deep_rates_remember(pplayer);
}
