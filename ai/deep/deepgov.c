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
#include "government.h"
#include "player.h"
#include "requirements.h"
#include "research.h"

/* server */
#include "cityturn.h"
#include "plrhand.h"

/* server/advisors */
#include "advdata.h"
#include "advtools.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"

#include "deepai.h"
#include "deephorizon.h"
#include "deeptuning.h"

#include "deepgov.h"

/* Government valuation (seam map section 4; ladder row 6a).
 *
 * `gov_value` is a vtable hook, so owning the decision costs no
 * `ai/default` change at all -- deep has simply never used the seam.
 *
 * WHAT STOCK DOES. `dai_gov_value()` sets each candidate government,
 * re-arranges every city under it, sums `dai_city_want()` over the
 * empire, adds the value of every effect the government sources at a
 * horizon of `turns = 9999` (its own comment: "TODO: Set to correct
 * value"), scales the total by two percentage bonuses and amortizes
 * by the number of techs still missing. That is a LEVEL, at today's
 * size, and the revolution that gets you there is free.
 *
 * WHAT DEEP DOES INSTEAD. The grammar the amortization audit asked
 * for (findings/amortization-audit-2026-08-07.md item 1d): a
 * government is a per-turn STREAM measured against the incumbent,
 * counted over `deep_horizon_turns()` -- the same MORT horizon every
 * other deep credit counts over -- against the one-off cost of
 * getting there. Concretely
 *
 *   value(gov) = (output(gov) - output(incumbent)) * horizon
 *                - anarchy cost
 *                + terms the arranged output cannot see
 *
 * all in the advisor's tile-output currency, which is the currency
 * `dai_city_want()`, `deeprates.c` and the horizon credits already
 * share. Differencing is what makes the anarchy charge meaningful:
 * a one-off cost is only comparable to a gain when the gain has a
 * horizon, and it is what makes "stay where we are" score exactly
 * zero rather than a large positive number that any other government
 * has to beat by accident.
 *
 * WHAT IS DELIBERATELY DROPPED. Stock's per-government effect sum.
 * The arrangement already prices everything a government does to
 * output and to happiness -- output bonuses and penalties, waste and
 * corruption, tithes, and (through the CM's own happiness
 * constraint) martial law, empire size and Make_Content. Valuing
 * those effects a second time in stock's want currency, at a
 * 9999-turn horizon, is RC4 in one function. What the drop genuinely
 * loses is the government effects with no output shadow at all --
 * veteran chance, HP regen, bribe cost, unit upkeep freedom and the
 * rate ceiling. The last two come back as their own terms below;
 * the military ones are named in STATUS as this valuation's known
 * blind spot, because pricing them honestly is a combat-currency
 * question and not a government one.
 *
 * THE CM COUPLING, NOW DELIBERATE. `auto_arrange_workers()` honours
 * `pcity->cm_parameter`, which deep sets and stock does not, so every
 * candidate government has silently been evaluated through deep's own
 * city manager since `deepcm.c` landed (seam map section 2.C: "not a
 * bug; not designed either"). It stays that way and is now on
 * purpose: deep's CM is how deep's cities will actually be run, so it
 * is the right arrangement to evaluate a government under.
 */

/**********************************************************************//**
  Empire output per turn under `gov`, in the advisor's want currency.

  Sets the government, lets the ruleset clamp the rates to it, and
  re-arranges every city -- exactly the sequence stock uses, because
  it is the only way to see what a government does to the tiles. The
  caller (`adv_best_government()`) restores the real government and
  re-arranges once the whole burst is over, which is why leaving
  `pplayer->government` set here is the existing contract and not a
  leak.

  The two percentage bonuses are stock's, applied to this
  government's own LEVEL rather than to a difference: "+25% for being
  immune to a class of action" is a statement about how good this
  government is, and multiplying a DELTA by it would make a bad
  government look worse the more immunities it granted.
**************************************************************************/
static double deep_gov_output(struct player *pplayer, struct adv_data *adv,
                              struct government *gov)
{
  double out = 0.0;
  double bonus;
  int revolution_turns;

  pplayer->government = gov;
  /* Stock's own comment here: re-optimising the tax rates per
   * candidate is too expensive. Deep does not re-optimise either;
   * what it does instead is price the CEILING each government sets,
   * which is the part of the rates question a government actually
   * decides. */
  check_player_max_rates(pplayer);
  city_list_iterate(pplayer->cities, acity) {
    auto_arrange_workers(acity);
  } city_list_iterate_end;

  city_list_iterate(pplayer->cities, pcity) {
    out += dai_city_want(pplayer, pcity, adv, NULL);
  } city_list_iterate_end;

  bonus = adv_gov_action_immunity_want(gov)
          + adv_gov_player_bonus_want(pplayer);
  revolution_turns = get_player_bonus(pplayer, EFT_REVOLUTION_UNHAPPINESS);
  if (revolution_turns > 0) {
    bonus -= 6.0 / revolution_turns;
  }
  out += out * bonus / 100.0;

  return out;
}

/**********************************************************************//**
  The incumbent's output, computed once per evaluation burst.

  `adv_best_government()` sets `adv->goal.revolution` to the real
  current government BEFORE its candidate loop and only re-points it
  afterwards, so it is the one readable place that still knows what
  we are actually governing under once the loop has started
  overwriting `pplayer->government`. Stamped by turn and by the
  incumbent itself, so a stale entry can never be read as a baseline.
**************************************************************************/
static double deep_gov_baseline(struct player *pplayer, struct adv_data *adv,
                                struct government *now)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  if (dp->gov_base_turn != game.info.turn
      || dp->gov_base_gov != government_number(now)) {
    dp->gov_base_out = deep_gov_output(pplayer, adv, now);
    dp->gov_base_turn = game.info.turn;
    dp->gov_base_gov = government_number(now);
  }

  return dp->gov_base_out;
}

/**********************************************************************//**
  Techs still missing before this government can be adopted at all.
  Stock's arithmetic, kept: a government behind three advances is
  worth less now than the same government behind none, and amortize()
  is the project's discounter for exactly that.
**************************************************************************/
static int deep_gov_tech_distance(const struct player *pplayer,
                                  struct government *gov)
{
  const struct research *presearch = research_get(pplayer);
  int dist = 0;

  requirement_vector_iterate(&gov->reqs, preq) {
    if (VUT_ADVANCE == preq->source.kind) {
      dist += MAX(1, research_goal_unknown_techs(presearch,
                       advance_number(preq->source.value.advance)));
    }
  } requirement_vector_iterate_end;

  return dist;
}

/**********************************************************************//**
  Value of `gov` to `pplayer`, in the advisor's want currency.

  The single caller is `adv_best_government()` (advdata.c), once per
  candidate government per re-evaluation burst (every 5-20 turns).
  `*override` tells the caller whether to skip its own valuation.
**************************************************************************/
void deep_gov_value(struct ai_type *ait, struct player *pplayer,
                    struct government *gov, adv_want *val, bool *override)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct adv_data *adv;
  struct government *now;
  double base, cand, value;
  int horizon;

  if (tune->gov_valuation == 0) {
    dai_gov_value(ait, pplayer, gov, val, override);
    return;
  }

  adv = adv_data_get(pplayer, NULL);
  if (adv == NULL || city_list_size(pplayer->cities) == 0) {
    /* Stock's own refusal: with no cities there is no empire output
     * to difference, and the advisor's default handling runs. */
    *override = FALSE;
    return;
  }

  now = (adv->goal.revolution != NULL
         ? adv->goal.revolution : government_of_player(pplayer));
  base = deep_gov_baseline(pplayer, adv, now);
  cand = deep_gov_output(pplayer, adv, gov);
  horizon = deep_horizon_turns();

  value = (cand - base) * horizon;

  *val = amortize(value, deep_gov_tech_distance(pplayer, gov));
  deep_plr_data(pplayer)->gov_valued++;

  log_base(deep_metric_log_level(),
           "deep-gov turn=%d player=\"%s\" gov=\"%s\" now=\"%s\""
           " base=%.1f out=%.1f delta=%.1f horizon=%d dist=%d val=%.1f",
           game.info.turn, player_name(pplayer),
           government_rule_name(gov), government_rule_name(now),
           base, cand, cand - base, horizon,
           deep_gov_tech_distance(pplayer, gov), (double) *val);

  *override = TRUE;
}
