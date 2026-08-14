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
#include "unit.h"
#include "unitlist.h"
#include "effects.h"
#include "game.h"
#include "government.h"
#include "player.h"
#include "requirements.h"
#include "research.h"

/* ai */
#include "handicaps.h"

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
#include "deeprates.h"
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
  What one point of `otype` is worth per turn, in the same currency
  and at the same prices `dai_city_want()` pays for the city's own
  output -- including its gold emphasis, which reads a tax rate above
  50 as evidence that the empire is short of gold. Reusing the city
  want's prices is the whole point: a shield paid to a unit and a
  shield produced by a tile have to be the same shield or the
  subtraction below means nothing.
**************************************************************************/
static double deep_gov_output_price(const struct player *pplayer,
                                    const struct adv_data *adv,
                                    Output_type_id otype)
{
  switch (otype) {
  case O_FOOD:
    return adv->food_priority;
  case O_SHIELD:
    return adv->shield_priority;
  case O_GOLD:
    if (pplayer->economic.tax > 50) {
      return adv->gold_priority * (pplayer->economic.tax - 40) / 14.0;
    }
    return adv->gold_priority;
  default:
    /* Nothing else is ever charged as unit upkeep. */
    return 0.0;
  }
}

/**********************************************************************//**
  The empire's unit upkeep bill under the government currently set.

  This is the half of "output and upkeep" that the arrangement cannot
  see. `dai_city_want()` says so itself -- "Unit upkeep isn't handled
  here" -- and it is precisely the half governments differ most on:
  under civ2civ3 the early governments charge almost no unit gold and
  Republic and Democracy charge for nearly everything, so a valuation
  built on arranged output alone systematically recommends the
  government whose bill it is not reading.

  A rule-3 copy of `update_unit_upkeep()`'s arithmetic
  (server/citytools.c:3119) rather than a call to it, for the reason
  that function exists: it WRITES `punit->upkeep[]` and sends packets.
  A counterfactual may not do either. `unit_upkeep_cost()` reads the
  owner's government through the effect system, so setting the
  candidate government above is what makes this the candidate's bill.
**************************************************************************/
static double deep_gov_unit_upkeep(struct player *pplayer,
                                   const struct adv_data *adv)
{
  double bill = 0.0;

  city_list_iterate(pplayer->cities, pcity) {
    int free_uk[O_LAST];

    output_type_iterate(o) {
      free_uk[o] = get_city_output_bonus(pcity, get_output_type(o),
                                         EFT_UNIT_UPKEEP_FREE_PER_CITY);
    } output_type_iterate_end;

    unit_list_iterate(pcity->units_supported, punit) {
      output_type_iterate(o) {
        int cost = unit_upkeep_cost(punit, o);

        if (cost > 0) {
          if (free_uk[o] > cost) {
            free_uk[o] -= cost;
            cost = 0;
          } else {
            cost -= free_uk[o];
            free_uk[o] = 0;
          }
        }
        bill += cost * deep_gov_output_price(pplayer, adv, o);
      } output_type_iterate_end;
    } unit_list_iterate_end;
  } city_list_iterate_end;

  return bill;
}

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
                              struct government *gov, double *upkeep)
{
  double out = 0.0;
  double bill;
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

  /* Net of what the standing army costs under THIS government. */
  bill = deep_gov_unit_upkeep(pplayer, adv);
  out -= bill;
  if (upkeep != NULL) {
    *upkeep = bill;
  }

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
static double deep_gov_baseline(struct ai_type *ait, struct player *pplayer,
                                struct adv_data *adv, struct government *now)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  if (dp->gov_base_turn != game.info.turn
      || dp->gov_base_gov != government_number(now)) {
    struct player_economic was = pplayer->economic;

    dp->gov_base_out = deep_gov_output(pplayer, adv, now, NULL);
    /* See deep_gov_anarchy_output(): an EXTRA pass may not leave the
     * player's rates anywhere stock's own passes would not. */
    pplayer->economic = was;
    dp->gov_base_turn = game.info.turn;
    dp->gov_base_gov = government_number(now);
    /* Whether a revolution costs anarchy at all is a property of the
     * player under the INCUMBENT government, which is what is set
     * right now and will not be again once the candidate loop moves
     * on. The server asks the same question in the same order. */
    dp->gov_anarchy_real = (get_player_bonus(pplayer, EFT_NO_ANARCHY) <= 0
                            && (!is_ai(pplayer)
                                || has_handicap(pplayer, H_REVOLUTION)));
    /* Same window, same reason: the rates model has to be read off
     * the empire as it stands, under the government it stands under,
     * before any candidate re-arranges it. */
    dp->gov_rate_model_ok
      = (deep_tuning_get()->gov_rate_ceiling_gain > 0.0
         && deep_rates_model(ait, pplayer, &dp->gov_rate_model));
  }

  return dp->gov_base_out;
}

/**********************************************************************//**
  Expected turns of anarchy on the way to `gov`, without drawing from
  the RNG.

  This is `revolentype_length()` (server/plrhand.c:515) with the
  random draws replaced by their means -- a rule-3 copy, and the
  reason it is one is that the original CALLS fc_rand(): an evaluation
  that consumes RNG state would make the valuation depend on how many
  candidates were scored, which is the purity property W26 exists to
  protect. `fc_rand(n) + 1` has mean (n + 1) / 2.

  The guard on `len - 1` is not in the original, where a `revolen` of
  1 (the setting's own minimum) with revolentype QUICKENING divides by
  zero. Recorded in findings/upstream-bugs.md; deep does not reproduce
  it.
**************************************************************************/
static double deep_revolen_expected(struct government *gov)
{
  int len = MAX(1, game.server.revolution_length);
  int change_speed, max_turns;

  switch (game.info.revolentype) {
  case REVOLEN_FIXED:
    return len;
  case REVOLEN_RANDOM:
    return (len + 1) / 2.0;
  case REVOLEN_QUICKENING:
  case REVOLEN_RANDQUICK:
    change_speed = MAX(1, (player_count() / 2) / MAX(1, len - 1));
    max_turns = MAX(1, len - gov->changed_to_times / change_speed);
    if (game.info.revolentype == REVOLEN_RANDQUICK) {
      return (max_turns + 1) / 2.0;
    }
    return max_turns;
  }

  return len;
}

/**********************************************************************//**
  Turns of anarchy this player would actually serve to reach `gov`.

  Mirrors `handle_player_change_government()`'s own decision, because
  a cost the server will not charge must not be priced -- and, equally,
  a cost it DOES charge must not be priced at zero.

  The floor is ONE turn, always. `H_REVOLUTION` and `EFT_NO_ANARCHY`
  decide `turns`, the number of EXTRA turns served; they do not decide
  whether the empire passes through anarchy at all.
  `handle_player_change_government()` sets
  `pplayer->government = game.government_during_revolution`
  unconditionally (server/plrhand.c:615) and then sets
  `revolution_finishes = game.info.turn + turns`, while the only call
  to `update_revolution()` is at BEGIN phase (server/srv_main.c:1381).
  So even with `turns == 0` the empire is under Anarchy when that
  turn's city production runs, and does not leave until the next
  turn begins. A revolution costs one full turn of output at every
  difficulty; more where anarchy is real.

  This was measured, not reasoned: across the 20 G3 lategame-80 games
  the savegame's own `changed_to_times` counter and the scorelog's
  government tag agree at exactly 1.00 turns of Anarchy per government
  change in 20/20 seeds (deep 26.8 changes and 26.8 anarchy turns per
  game; stock 17.8 and 17.9).

  A player who is ALREADY in anarchy needs no special case: the turn
  is sunk, and `base` is then the anarchy output itself, so the
  `MAX(0.0, base - anarchy_out)` in the caller is zero and the charge
  vanishes on its own.
**************************************************************************/
static double deep_gov_anarchy_turns(struct player *pplayer,
                                     struct government *gov)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  if (pplayer->revolution_finishes > game.info.turn) {
    /* Already paying for one; the server charges the remainder only. */
    return pplayer->revolution_finishes - game.info.turn;
  }
  if (!dp->gov_anarchy_real) {
    /* No EXTRA turns, but the pass through anarchy still happens. */
    return 1.0;
  }

  return MAX(1.0, deep_revolen_expected(gov));
}

/**********************************************************************//**
  Empire output per turn under the anarchy the revolution passes
  through, computed once per burst and only when anarchy is real.

  The rates are snapshotted around it. `check_player_max_rates()` does
  not just read a ceiling -- it CLAMPS the player's live rates down to
  it and notifies the player it did. Anarchy's ceiling is the lowest
  in most rulesets, so pricing the detour would otherwise leave the
  empire's tax split squeezed to anarchy's cap for the rest of the
  phase, and post a message about it, for a government we are only
  imagining. Stock's candidate loop leaves the same trace and this
  file cannot fix that from here; what it can do is not ADD one.
**************************************************************************/
static double deep_gov_anarchy_output(struct player *pplayer,
                                      struct adv_data *adv)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  if (dp->gov_anarchy_turn != game.info.turn) {
    struct player_economic was = pplayer->economic;

    dp->gov_anarchy_out = deep_gov_output(pplayer, adv,
                                          game.government_during_revolution,
                                          NULL);
    pplayer->economic = was;
    dp->gov_anarchy_turn = game.info.turn;
  }

  return dp->gov_anarchy_out;
}

/**********************************************************************//**
  What `gov`'s rate ceiling is worth, over and above the incumbent's.

  A government DECIDES a ceiling -- Despotism 60, Democracy 90 under
  the classic rulesets -- and nothing in the tree has ever priced one.
  `EFT_MAX_RATES` returns 0 from `dai_effect_value()`, and both
  `daidata.c:547` and `advdata.c:1031` carry in-source admissions that
  re-optimising the rates per candidate government is too expensive
  (findings/darkfx-2026-08-06.md item 5).

  Deep does not re-optimise the empire either. It re-runs one thing:
  the rates enumeration `deeprates.c` already owns, over the SAME
  model of the SAME empire, with only the ceiling changed. The
  difference between the two winners is what the ceiling is worth --
  which is automatically ZERO when the split is interior, i.e. when
  the empire is not up against its ceiling at all. That is the trap
  darkfx named: an empire that cannot use a higher cap must not be
  sold one, and reading the binding constraint rather than the nominal
  ceiling is how the term avoids it. No extra condition does it; the
  arithmetic does.

  The model is built ONCE per burst, under the incumbent, before the
  candidate loop starts re-arranging cities: it reads live outputs
  (last turn's bulbs, this turn's luxury per city), and rebuilding it
  under a candidate's arrangement would measure the candidate twice.

  `deep_rates_pick()` values a split over MORT already, so the two
  winners' difference is a horizon total and commensurable with the
  stream term by construction.
**************************************************************************/
static double deep_gov_ceiling_gain(struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  struct deep_rate_model cand;
  int ceiling;

  if (!dp->gov_rate_model_ok) {
    return 0.0;
  }
  ceiling = deep_rates_max_rate(pplayer);
  if (ceiling == dp->gov_rate_model.maxrate) {
    return 0.0;
  }

  cand = dp->gov_rate_model;
  cand.maxrate = ceiling;

  return deep_rates_pick(&cand).value
         - deep_rates_pick(&dp->gov_rate_model).value;
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
  double anarchy_turns = 0.0, charge = 0.0, upkeep = 0.0;
  double ceiling = 0.0;
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
  base = deep_gov_baseline(ait, pplayer, adv, now);

  /* The revolution's own bill: the turns the empire spends under the
   * anarchy government instead of the one it has, at the difference
   * in what it produces. An upfront cost against a per-turn stream,
   * which is only a comparison at all because the stream above has a
   * horizon. Never negative: anarchy that OUT-produces the incumbent
   * is a measurement artefact, not a reason to revolt. */
  anarchy_turns = (gov == now ? 0.0
                   : deep_gov_anarchy_turns(pplayer, gov));
  if (anarchy_turns > 0.0) {
    double anarchy_out = deep_gov_anarchy_output(pplayer, adv);

    charge = anarchy_turns * MAX(0.0, base - anarchy_out)
             * tune->gov_anarchy_weight;
    if (charge > 0.0) {
      deep_plr_data(pplayer)->gov_anarchy_charged++;
    }
  }

  cand = deep_gov_output(pplayer, adv, gov, &upkeep);
  horizon = deep_horizon_turns();

  /* The candidate government is the one set right now, which is what
   * makes this its ceiling and not the incumbent's. */
  if (tune->gov_rate_ceiling_gain > 0.0) {
    ceiling = deep_gov_ceiling_gain(pplayer) * tune->gov_rate_ceiling_gain;
    if (ceiling != 0.0) {
      deep_plr_data(pplayer)->gov_ceiling_paid++;
    }
  }

  value = (cand - base) * horizon - charge + ceiling;

  *val = amortize(value, deep_gov_tech_distance(pplayer, gov));
  deep_plr_data(pplayer)->gov_valued++;

  log_base(deep_metric_log_level(),
           "deep-gov turn=%d player=\"%s\" gov=\"%s\" now=\"%s\""
           " base=%.1f out=%.1f upk=%.1f delta=%.1f horizon=%d anturns=%.1f"
           " ancost=%.1f ceil=%.1f dist=%d val=%.1f",
           game.info.turn, player_name(pplayer),
           government_rule_name(gov), government_rule_name(now),
           base, cand, upkeep, cand - base, horizon, anarchy_turns, charge,
           ceiling,
           deep_gov_tech_distance(pplayer, gov), (double) *val);

  *override = TRUE;
}
