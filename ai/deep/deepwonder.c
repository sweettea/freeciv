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
#include "actions.h"
#include "city.h"
#include "game.h"
#include "improvement.h"
#include "player.h"
#include "unittype.h"
#include "requirements.h"
#include "research.h"

/* server/advisors */
#include "advdata.h"
#include "infracache.h"
#include "advtools.h"

/* ai/default */
#include "daicity.h"
#include "daiplayer.h"

#include "deepai.h"
#include "deeptuning.h"

#include "deepwonder.h"

/**********************************************************************//**
  Turns of OUR OWN research until `goal` is known.

  Own research only, and that is the human's framing of 2026-08-05:
  "bulbs divided by rate toward a named tech -- readable, no rival
  modelling". Stock's own obsolescence estimator (daicity.c:1701-1715)
  takes a MIN over every player's bulb rate, which is a rival model
  wearing a minimum sign, and it is not what either consumer here
  wants: the obsolescence that matters to a building we are about to
  pay for is the obsolescence OUR queue is about to cause.

  `bulbs_last_turn` is the rate every other estimator in this tree
  reads. -1 for "not on the way": already known, unreachable, or no
  bulbs at all, in which case the truncation must not fire.
**************************************************************************/
int deep_tech_eta(const struct player *pplayer, Tech_type_id goal)
{
  const struct research *presearch = research_get(pplayer);
  int bulbs, rate;

  if (!valid_advance_by_number(goal)
      || research_invention_state(presearch, goal) == TECH_KNOWN) {
    return -1;
  }
  if (research_goal_unknown_techs(presearch, goal) <= 0) {
    return -1;                  /* Unreachable in this ruleset. */
  }
  bulbs = research_goal_bulbs_required(presearch, goal)
          - presearch->bulbs_researched;
  if (bulbs <= 0) {
    return 0;
  }
  rate = pplayer->server.bulbs_last_turn;
  if (rate <= 0) {
    return -1;                  /* No research, no arrival to estimate. */
  }

  return (bulbs + rate - 1) / rate;
}

/**********************************************************************//**
  Turns until this improvement goes obsolete FOR US.

  WHAT OBSOLESCENCE ACTUALLY DOES, verified rather than assumed (the
  brief asked for exactly this):

  - An obsolete IMPROVEMENT is AUTO-SOLD by the server.
    remove_obsolete_buildings_city() (server/cityturn.c:236) sells
    every built improvement whose obsolete_by is satisfied, for
    impr_sell_gold(), and notifies the owner. So the shields are gone
    and a fraction comes back as gold.
  - An obsolete WONDER is NOT sold: is_building_sellable() is
    is_improvement(), which is false for both wonder genera
    (common/improvement.c:1122). The monument stands and pays nothing.
  - EITHER WAY THE EFFECTS LAPSE. A VUT_IMPROVEMENT requirement whose
    building is obsolete evaluates TRI_NO (common/requirements.c:2360),
    so every effect the building sources stops firing, and
    can_city_build_improvement_now() refuses to build it at all
    (common/city.c:868).

  WHOSE RESEARCH TRIGGERS IT: the OWNER's. improvement_obsolete()
  builds a req_context carrying `.player = pplayer` and nothing wider
  (common/improvement.c:413), and callers pass city_owner(). A ruleset
  could write the requirement at REQ_RANGE_WORLD and make it everyone's
  -- the requirement vector permits it -- and neither shipped ruleset
  does: all 16 civ2civ3 obsolete_by tech requirements are Player range.
  This function prices Player-range present Tech requirements and skips
  everything else, which is honest for both shipped rulesets and
  silently declines to guess for one that is not.

  The MINIMUM over the vector, because obsolete_by is a disjunction:
  any one satisfied requirement obsoletes the building.
**************************************************************************/
int deep_impr_obsolete_eta(const struct player *pplayer,
                           const struct impr_type *pimprove)
{
  int best = -1;

  if (!can_improvement_go_obsolete(pimprove)) {
    return -1;
  }

  requirement_vector_iterate(&pimprove->obsolete_by, preq) {
    if (VUT_ADVANCE == preq->source.kind && preq->present
        && REQ_RANGE_PLAYER == preq->range) {
      int eta = deep_tech_eta(pplayer,
                             advance_number(preq->source.value.advance));

      if (eta >= 0 && (best < 0 || eta < best)) {
        best = eta;
      }
    }
  } requirement_vector_iterate_end;

  return best;
}

/**********************************************************************//**
  Our own completion ETA for this wonder in this city.

  city_turns_to_build() with the shield stock counted, which is the
  same arithmetic the production dialogue shows and the same one
  deep_build_eta() uses on the unit side. -1 when the city makes no
  shields, because an infinite ETA is not a race position, it is an
  absence from the race.
**************************************************************************/
int deep_wonder_own_eta(struct city *pcity,
                        const struct impr_type *pimprove)
{
  struct universal target = { .kind = VUT_IMPROVEMENT,
                              .value = { .building = pimprove } };
  int turns;

  if (pcity->surplus[O_SHIELD] <= 0) {
    return -1;
  }
  turns = city_turns_to_build(pcity, &target, TRUE);
  if (turns >= FC_INFINITY) {
    return -1;
  }

  return turns;
}

/**********************************************************************//**
  THE ONE FUNCTION W04 SUBSTITUTES.

  Every other alive player's cities, their current production and their
  shield stock: the same omniscient reads the W36 order census took,
  and the same class of read stock's own city worth already makes. The
  best (smallest) completion ETA among cities actually building this
  wonder right now.

  Deliberately NOT "cities that could build it": a city that has not
  started is not in the race, and counting it would make every rival
  empire a permanent competitor for every wonder. The cost of that
  choice is that a rival who starts tomorrow is invisible today, which
  is a real error and is exactly what the calibration counter
  (races LOST at high estimated margin) exists to expose.

  Under the honesty flag this becomes a known_city read with its own
  staleness, and everything above it inherits that through this seam
  and no other. That is why the read is one function and not four
  inline loops.
**************************************************************************/
int deep_wonder_rival_eta(const struct player *pplayer,
                          const struct impr_type *pimprove,
                          const struct player **who_out)
{
  int best = -1;

  if (who_out != NULL) {
    *who_out = NULL;
  }

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || is_barbarian(aplayer)) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      int eta;

      if (VUT_IMPROVEMENT != acity->production.kind
          || acity->production.value.building != pimprove) {
        continue;
      }
      eta = deep_wonder_own_eta(acity, pimprove);
      if (eta < 0) {
        continue;
      }
      if (best < 0 || eta < best) {
        best = eta;
        if (who_out != NULL) {
          *who_out = aplayer;
        }
      }
    } city_list_iterate_end;
  } players_iterate_alive_end;

  return best;
}

/**********************************************************************//**
  P(we finish this wonder first).

  A linear ramp on the margin in turns, with wrace_margin_turns as its
  scale: dead level is 0.5, winning by the whole margin is 1.0, losing
  by it is 0.0. One knob and one shape, and the shape is the honest
  statement of what a turn-count estimate can support -- neither
  estimate is precise enough to justify a curve, and a step function
  would make the want jump on a one-turn wobble in a rival's surplus.

  No rival building it at all is 1.0 and not 0.999: the race term must
  not tax an uncontested wonder, which is the whole point of ranking
  P(win) x want above want.
**************************************************************************/
double deep_wonder_p_win(struct city *pcity,
                         const struct impr_type *pimprove,
                         int *ours_out, int *theirs_out,
                         const struct player **who_out)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct player *who = NULL;
  int ours = deep_wonder_own_eta(pcity, pimprove);
  int theirs = deep_wonder_rival_eta(city_owner(pcity), pimprove, &who);
  double p, scale = MAX(1, tune->wrace_margin_turns);

  if (ours_out != NULL) {
    *ours_out = ours;
  }
  if (theirs_out != NULL) {
    *theirs_out = theirs;
  }
  if (who_out != NULL) {
    *who_out = who;
  }

  if (theirs < 0) {
    return 1.0;                 /* Uncontested. */
  }
  if (ours < 0) {
    return 0.0;                 /* We are not in this race at all. */
  }

  p = 0.5 + 0.5 * (theirs - ours) / scale;

  return CLIP(0.0, p, 1.0);
}

/**********************************************************************//**
  When our own research makes this unit type's successor buildable.

  The unit-side twin of deep_impr_obsolete_eta(), and the shapes differ
  because the ruleset states them differently: an improvement carries a
  requirement VECTOR that obsoletes it (a disjunction, so MINIMUM),
  while a unit carries a single `obsoleted_by` pointer and the
  question becomes when that successor's OWN build requirements are
  met (a conjunction, so MAXIMUM).

  -1 for "no successor imminent": no obsoleted_by, a successor already
  buildable, or any requirement we cannot put a number on -- in which
  case the consumer must not defer. Silence is not evidence.
**************************************************************************/
int deep_utype_successor_eta(const struct player *pplayer,
                             const struct unit_type *ut,
                             const struct unit_type **next_out)
{
  const struct unit_type *next = ut->obsoleted_by;
  int worst = 0;

  if (next_out != NULL) {
    *next_out = next;
  }
  if (next == U_NOT_OBSOLETED) {
    return -1;
  }
  if (can_player_build_unit_direct(pplayer, next, RPT_CERTAIN, TRUE)) {
    return -1;                  /* Already here: nothing to wait for. */
  }

  requirement_vector_iterate(&next->build_reqs, preq) {
    if (VUT_ADVANCE == preq->source.kind && preq->present) {
      Tech_type_id goal = advance_number(preq->source.value.advance);

      if (research_invention_state(research_get(pplayer), goal)
          != TECH_KNOWN) {
        int eta = deep_tech_eta(pplayer, goal);

        if (eta < 0) {
          return -1;            /* Unreachable or unestimable. */
        }
        worst = MAX(worst, eta);
      }
    }
  } requirement_vector_iterate_end;

  return worst > 0 ? worst : -1;
}

/* THE RACE LEDGER. One entry per (player, wonder) we have priced,
 * carrying the last estimate taken, resolved when the wonder stops
 * being available. The CALIBRATION metric the brief names is
 * "races LOST anyway": a loss recorded at a high estimated margin
 * means the estimator is wrong, and no amount of tuning the margin
 * fixes an estimator. Not saved. */
struct deep_race_memo {
  int plr_no;
  int impr_id;
  int first_turn;
  int ours;
  int theirs;
  double p;
  bool declined;
};

static struct deep_race_memo *deep_races = NULL;
static int deep_race_n = 0, deep_race_cap = 0;

/**********************************************************************//**
  Remember, or refresh, this player's position in one race.
**************************************************************************/
static void deep_race_note(const struct player *pplayer,
                           const struct impr_type *pimprove,
                           int ours, int theirs, double p, bool declined)
{
  struct deep_race_memo *e;
  int i;

  for (i = 0; i < deep_race_n; i++) {
    e = &deep_races[i];
    if (e->plr_no == player_number(pplayer)
        && e->impr_id == improvement_number(pimprove)) {
      e->ours = ours;
      e->theirs = theirs;
      e->p = p;
      e->declined = declined;
      return;
    }
  }
  if (deep_race_n >= deep_race_cap) {
    deep_race_cap = MAX(32, deep_race_cap * 2);
    deep_races = fc_realloc(deep_races,
                            deep_race_cap * sizeof(*deep_races));
  }
  e = &deep_races[deep_race_n++];
  e->plr_no = player_number(pplayer);
  e->impr_id = improvement_number(pimprove);
  e->first_turn = game.info.turn;
  e->ours = ours;
  e->theirs = theirs;
  e->p = p;
  e->declined = declined;
}

/**********************************************************************//**
  Close out every race of this player's whose wonder has been built.
  `won=1` means we own it; `won=0` with a high `p` is the calibration
  failure the brief asks to be shown rather than argued about.
**************************************************************************/
static void deep_race_adjudicate(const struct player *pplayer)
{
  int i = 0;

  while (i < deep_race_n) {
    struct deep_race_memo *e = &deep_races[i];
    const struct impr_type *pimprove;

    if (e->plr_no != player_number(pplayer)) {
      i++;
      continue;
    }
    pimprove = improvement_by_number(e->impr_id);
    if (pimprove == NULL || great_wonder_is_available(pimprove)) {
      i++;
      continue;                 /* Still open. */
    }
    log_base(deep_metric_log_level(),
             "deep-wrace-end turn=%d player=\"%s\" wonder=\"%s\""
             " opened=%d ours=%d theirs=%d p=%.3f declined=%d won=%d",
             game.info.turn, player_name(pplayer),
             improvement_rule_name(pimprove), e->first_turn, e->ours,
             e->theirs, e->p, e->declined ? 1 : 0,
             great_wonder_owner(pimprove) == pplayer ? 1 : 0);
    deep_races[i] = deep_races[--deep_race_n];
  }
}

/**********************************************************************//**
  THE WONDER-CITY ADMISSION CENSUS. Read-only, log-only.

  The race pass can only price candidates that exist, and a candidate
  only exists when `adv->wonder_city` names a city -- because
  dai_build_adv_adjust() zeroes EVERY wonder want in EVERY city when
  the wonder city is NULL (daicity.c:2007). So "deep built no wonders"
  has two completely different causes that the race counters cannot
  tell apart, and only one of them is about racing:

  (a) the pass priced candidates and something damped them, or
  (b) there was never a wonder city, so there was never a candidate.

  (b) is decided in server/advisors/advbuilding.c:209-254, by a filter
  this function REPLICATES READ-ONLY so the log can say which clause
  emptied the field:

  - `consider_wonder_city` rejects a city with grave_danger > 0
    (daicity.c:2086-2094 -- deep delegates to it verbatim);
  - with a HelpWonder unit in the ruleset (`has_help`), a candidate
    must ALSO sit on a continent carrying more than 5 of our cities
    AND have downtown > 3.

  Replicating rather than exporting is deliberate: advbuilding.c is
  shared with human auto-workers (CLAUDE.md rule 4) and this is an
  instrument, not a consumer. It is counted, never acted on.

  `downtown` is refreshed by calculate_city_clusters() only on the
  turns advbuilding.c actually looks for a new wonder city, so the
  count is exact on those turns and a lower bound on the others. Said
  here so the log is not read as more precise than it is.
**************************************************************************/
static void deep_wonder_city_census(struct ai_type *ait,
                                    const struct player *pplayer,
                                    const struct adv_data *adv,
                                    bool has_help, int *danger_out,
                                    int *eligible_out)
{
  int danger = 0, eligible = 0;

  city_list_iterate(pplayer->cities, pcity) {
    Continent_id place = tile_continent(pcity->tile);

    if (def_ai_city_data(pcity, ait)->grave_danger > 0) {
      danger++;
      continue;
    }
    if (has_help
        && !(place >= 0 && adv->stats.cities[place] > 5
             && pcity->server.adv->downtown > 3)) {
      continue;
    }
    eligible++;
  } city_list_iterate_end;

  *danger_out = danger;
  *eligible_out = eligible;
}

/**********************************************************************//**
  How many great wonders are ours, rivals', and still unbuilt.

  The outcome side of the ledger, and the one number the live-play flag
  ("15 cities, 1 wonder") is actually about. deep-wrace-end only closes
  races that were PRICED at least once, so a wonder deep never had a
  chance to want never appears there; this census is unconditional.
**************************************************************************/
static void deep_wonder_stock_census(const struct player *pplayer,
                                     int *ours_out, int *theirs_out,
                                     int *open_out)
{
  int ours = 0, theirs = 0, open = 0;

  improvement_iterate(pimprove) {
    if (!is_great_wonder(pimprove)) {
      continue;
    }
    if (great_wonder_is_built(pimprove)) {
      if (great_wonder_owner(pimprove) == pplayer) {
        ours++;
      } else {
        theirs++;
      }
    } else if (great_wonder_is_available(pimprove)) {
      open++;
    }
    /* Destroyed wonders are none of the three. */
  } improvement_iterate_end;

  *ours_out = ours;
  *theirs_out = theirs;
  *open_out = open;
}

/**********************************************************************//**
  The per-player wonder-race pass.

  Runs after stock's building_advisor() has chosen the wonder city and
  after dai_build_adv_adjust() has zeroed every wonder want OUTSIDE it,
  so the population here is exactly the wonder city's own candidates --
  which is what makes item 3 "one honest correction" rather than a
  wonder chooser: the candidate set and its base ranking are stock's
  and deep's existing effect wants, and only the race term is added.

  THE COMPOSED VALUATION (human formula, 2026-08-05):

    P(win) x SUM_empire(amortized effect value) x amortize(completion
    ETA, truncated at min(expected obsolescence, end_turn))

  The empire-scoped effect sum arrives from DARKFX and the endgame
  clamp from ENDGAME; the three factors this session owns are

  - P(win), above;
  - the COMPLETION-ETA DISCOUNT. A forty-turn build must not be paid
    for out of today's budget at face value. amortize() is the
    project's own delivery discount and the shape R2 already uses for
    a caravan that has to walk.
  - OBSOLESCENCE TRUNCATION. A wonder five turns from the advance that
    voids it is currently priced as eternal. deep_impr_obsolete_eta()
    says when our own queue voids it; a wonder whose obsolescence
    arrives BEFORE we could finish is worth nothing at all, and one
    that arrives soon after is worth the fraction of its life we get.

  All three gains are PLACEHOLDERs at 0/1 = off, and at those defaults
  the pass computes the estimate, logs it and writes no want.
**************************************************************************/
void deep_wonder_race(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  bool has_help
    = (num_role_units(action_id_get_role(ACTION_HELP_WONDER)) > 0);
  const struct impr_type *best_impr = NULL;
  struct city *wcity;
  adv_want best_want = 0;
  int declined = 0, priced = 0, abandoned = 0;
  int danger = 0, eligible = 0, own = 0, rival = 0, open = 0;

  deep_race_adjudicate(pplayer);

  if (adv == NULL) {
    return;
  }

  /* The admission census and the wonder stock are logged whether or not
   * a wonder city exists, because "no wonder city" is the hypothesis
   * they are here to separate from "wonder city, damped candidates". */
  deep_wonder_city_census(ait, pplayer, adv, has_help, &danger, &eligible);
  deep_wonder_stock_census(pplayer, &own, &rival, &open);

  wcity = (adv->wonder_city > 0
           ? player_city_by_number(pplayer, adv->wonder_city) : NULL);

  if (wcity == NULL) {
    log_base(deep_metric_log_level(),
             "deep-wrace-city turn=%d player=\"%s\" city=-1 cities=%d"
             " hashelp=%d danger=%d eligible=%d size=-1 surplus=-1"
             " stock=-1 prod=\"-\" building=0 cands=0 best=\"-\""
             " bestwant=0.0 own=%d rival=%d open=%d",
             game.info.turn, player_name(pplayer),
             city_list_size(pplayer->cities), has_help ? 1 : 0, danger,
             eligible, own, rival, open);
    return;
  }

  improvement_iterate(pimprove) {
    Impr_type_id id = improvement_number(pimprove);
    adv_want *want = &wcity->server.adv->building_want[id];
    const struct player *who = NULL;
    double p, factor = 1.0;
    /* The three terms of factor= , kept apart on the line because the
     * knob census cannot attribute a folded product to one of three
     * knobs (benchmark/knobcensus.py, wrace_pwin_gain: "Needs: per-term
     * factors on deep-wrace"). Each is exactly 1.0 when its gain is
     * off, and factor is still accumulated in the original order so
     * the arithmetic is bit-for-bit what it was. */
    double pwin_f = 1.0, eta_f = 1.0, obs_f = 1.0;
    int ours = -1, theirs = -1, ob_eta;
    bool decline;

    if (!is_great_wonder(pimprove) || *want <= 0
        || !can_city_build_improvement_now(wcity, pimprove, RPT_CERTAIN)) {
      continue;
    }

    p = deep_wonder_p_win(wcity, pimprove, &ours, &theirs, &who);
    ob_eta = deep_impr_obsolete_eta(pplayer, pimprove);

    /* Step 1, the go/no-go: losing by more than the margin is p == 0
     * by construction of the ramp, and so is "obsolete before we could
     * finish". Both are the same verdict -- the shields buy nothing --
     * and both are counted as a decline. */
    decline = (p <= 0.0)
              || (ob_eta >= 0 && ours >= 0 && ob_eta <= ours);

    if (tune->wrace_pwin_gain > 0.0) {
      pwin_f = 1.0 - tune->wrace_pwin_gain * (1.0 - p);
      factor *= pwin_f;
    }
    if (tune->wrace_eta_gain > 0.0 && ours > 0) {
      double disc = (double) amortize((adv_want) 1000, ours) / 1000.0;

      eta_f = 1.0 - tune->wrace_eta_gain * (1.0 - disc);
      factor *= eta_f;
    }
    if (tune->wrace_obsolete_gain > 0.0 && ob_eta >= 0) {
      /* The share of a MORT horizon the wonder actually lives for,
       * counted from the turn it would arrive. Zero when it dies
       * first, which is the truncation the formula names. */
      double life = MAX(0, ob_eta - MAX(ours, 0));
      double share = MIN(1.0, life / (double) MORT);

      obs_f = 1.0 - tune->wrace_obsolete_gain * (1.0 - share);
      factor *= obs_f;
    }
    factor = MAX(factor, 0.0);

    if (*want > best_want) {
      best_want = *want;
      best_impr = pimprove;
    }

    log_base(deep_metric_log_level(),
             "deep-wrace turn=%d player=\"%s\" city=%d wonder=\"%s\""
             " ours=%d theirs=%d rival=\"%s\" p=%.3f obsolete=%d"
             " want=%.1f factor=%.3f pwin_f=%.3f eta_f=%.3f obs_f=%.3f"
             " decline=%d",
             game.info.turn, player_name(pplayer), wcity->id,
             improvement_rule_name(pimprove), ours, theirs,
             who != NULL ? player_name(who) : "-", p, ob_eta,
             (double) *want, factor, pwin_f, eta_f, obs_f,
             decline ? 1 : 0);

    deep_race_note(pplayer, pimprove, ours, theirs, p, decline);
    if (decline) {
      declined++;
      /* ABANDONMENT, counted separately from declining to start: the
       * shields are already in the stock and the salvage is what a
       * switch recovers, which is deep's switch-cost machinery's
       * question and not this pass's. What this pass owes the
       * decision is a want of zero and a number saying it happened. */
      if (VUT_IMPROVEMENT == wcity->production.kind
          && wcity->production.value.building == pimprove) {
        abandoned++;
        log_base(deep_metric_log_level(),
                 "deep-wrace-abandon turn=%d player=\"%s\" city=%d"
                 " wonder=\"%s\" ours=%d theirs=%d p=%.3f obsolete=%d"
                 " stock=%d",
                 game.info.turn, player_name(pplayer), wcity->id,
                 improvement_rule_name(pimprove), ours, theirs, p,
                 ob_eta, wcity->shield_stock);
      }
    } else {
      priced++;
    }

    if (factor != 1.0) {
      *want = (adv_want) (*want * factor);
    }
  } improvement_iterate_end;

  if (priced + declined > 0) {
    log_base(deep_metric_log_level(),
             "deep-wrace-sum turn=%d player=\"%s\" city=%d priced=%d"
             " declined=%d abandoned=%d",
             game.info.turn, player_name(pplayer), wcity->id, priced,
             declined, abandoned);
  }

  /* Unconditional, unlike deep-wrace-sum: a turn with a wonder city and
   * ZERO candidates is the observation that matters most, and the sum
   * line is exactly the line that does not exist on it. `bestwant` is
   * the largest candidate want BEFORE the race factor is applied, so
   * the pair (bestwant, factor= on the deep-wrace lines) separates "the
   * effect wants were small" from "the race pass shrank them". */
  log_base(deep_metric_log_level(),
           "deep-wrace-city turn=%d player=\"%s\" city=%d cities=%d"
           " hashelp=%d danger=%d eligible=%d size=%d surplus=%d"
           " stock=%d prod=\"%s\" building=%d cands=%d best=\"%s\""
           " bestwant=%.1f own=%d rival=%d open=%d",
           game.info.turn, player_name(pplayer), wcity->id,
           city_list_size(pplayer->cities), has_help ? 1 : 0, danger,
           eligible, city_size_get(wcity), wcity->surplus[O_SHIELD],
           wcity->shield_stock, universal_rule_name(&wcity->production),
           (VUT_IMPROVEMENT == wcity->production.kind
            && is_great_wonder(wcity->production.value.building)) ? 1 : 0,
           priced + declined,
           best_impr != NULL ? improvement_rule_name(best_impr) : "-",
           (double) best_want, own, rival, open);
}

/**********************************************************************//**
  Race urgency for the help-wonder caravan.

  Symmetric to the go/no-go and the reason R2b is this session's
  delivery arm: a finish line is where a delivered caravan pays most,
  because the shields it carries decide the race rather than merely
  advancing a build nobody contests. Rises with how CLOSE the race is
  and only while we are the ones ahead -- feeding a race we are losing
  is the go/no-go's business, not this one's.

  1.0 whenever the gain is off, there is no rival, or we trail.
**************************************************************************/
double deep_wonder_race_urgency(struct city *wonder_city,
                                const struct impr_type *pimprove)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int ours = -1, theirs = -1;
  double p;

  if (tune->wrace_urgency_gain <= 0.0 || wonder_city == NULL
      || !is_great_wonder(pimprove)) {
    return 1.0;
  }
  p = deep_wonder_p_win(wonder_city, pimprove, &ours, &theirs, NULL);
  if (theirs < 0 || ours < 0 || p <= 0.5) {
    return 1.0;
  }

  /* Closeness: 1 at a dead heat, 0 once we lead by the whole margin.
   * The multiplier is largest where the race is tightest AND we are
   * ahead, which is exactly the window a caravan decides. */
  return 1.0 + tune->wrace_urgency_gain * (1.0 - (p - 0.5) * 2.0);
}
