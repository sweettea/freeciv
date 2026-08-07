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
#include "fcintl.h"
#include "log.h"
#include "support.h"

/* common */
#include "city.h"
#include "diptreaty.h"
#include "game.h"
#include "packets.h"
#include "player.h"
#include "research.h"
#include "tech.h"

/* server */
#include "diplhand.h"
#include "srv_log.h"

/* server/advisors */
#include "advdata.h"
#include "advtools.h"

/* ai */
#include "handicaps.h"

/* ai/default */
#include "daidata.h"
#include "daitools.h"

#include "deepai.h"
#include "deepdefence.h"
#include "deepops.h"
#include "deeptrade.h"
#include "deeptuning.h"

#include "deepdiplomacy.h"

/* The cease-fire window, in turns left before the pact lapses back to
 * war. NOT a deep constant and not tunable: it is ai/default's own
 * refusal bar, `ds->turns_left > 4` at daidiplomacy.c:405, and deep
 * reads it so its proposal lands on a turn the counterparty's
 * valuation is allowed to say yes on. Changing it here would not
 * change the bar; it would only make deep propose into a refusal. */
#define DEEP_PEACE_WINDOW 4

/* Bulbs to gold. ai/default's own rate: dai_goldequiv_tech
 * (daidiplomacy.c:158-179) prices a tech at 3 x the bulbs it takes.
 * Reused rather than re-minted so the embassy clause is denominated
 * in the same gold the rest of the treaty balance is in -- a clause
 * valuation that invented its own exchange rate could not be added to
 * the others. */
#define DEEP_BULBS_TO_GOLD 3.0

/**********************************************************************//**
  Is any player we are at war with allied to `them`?

  A copy of ai/default's get_allied_with_enemy_player()
  (daidiplomacy.c:266-282), which is static. Copied rather than made
  visible because this session is under a zero-`ai/default`-changes
  brief; it is eleven lines with no state, and deep must apply exactly
  the guard the counterparty's own valuation applies or it will
  propose into a -BIG_NUMBER refusal every turn of the window.
**************************************************************************/
static const struct player *deep_allied_with_enemy(const struct player *us,
                                                   const struct player *them)
{
  players_iterate_alive(aplayer) {
    if (aplayer != us
        && aplayer != them
        && pplayers_allied(them, aplayer)
        && player_diplstate_get(us, aplayer)->type == DS_WAR) {
      return aplayer;
    }
  } players_iterate_alive_end;

  return nullptr;
}

/**********************************************************************//**
  Strip any clauses left on an open meeting between the two players.

  A copy of ai/default's clear_old_treaty() (daidiplomacy.c:1227-1243)
  for the same reason as above. Stock calls it before every suggestion
  it makes; deep's pre-pass runs BEFORE stock's pass on the same turn,
  so without it deep's peace clause could be added to a meeting still
  carrying last turn's refused offer and be judged together with it.
**************************************************************************/
static void deep_treaty_clear(struct player *pplayer, struct player *aplayer)
{
  struct treaty *old_treaty = find_treaty(pplayer, aplayer);

  if (old_treaty != nullptr) {
    clause_list_iterate(old_treaty->clauses, pclause) {
      dlsend_packet_diplomacy_remove_clause(aplayer->connections,
                                            player_number(pplayer),
                                            player_number(pclause->from),
                                            pclause->type, pclause->value);
      free(pclause);
    } clause_list_iterate_end;
    clause_list_destroy(old_treaty->clauses);
    old_treaty->clauses = clause_list_new();
  }
}

/**********************************************************************//**
  Suggest one clause. `to_pplayer` means aplayer is the giver.

  THE SEAM THE BRIEF ASKED ABOUT. ai/default's dai_diplomacy_suggest()
  (daidiplomacy.c:937-954) is static, but it is a three-line wrapper
  over `could_meet_with_player()` plus two server handlers that are
  already public (build/hand_gen.h). So deep reaches the proposal
  machinery without ai/default changing at all -- there is no seam to
  report. Both handlers call call_treaty_evaluate() on BOTH sides
  synchronously, so a proposal is judged inside this call.
**************************************************************************/
static void deep_treaty_suggest(struct player *pplayer,
                                struct player *aplayer,
                                enum clause_type what,
                                bool to_pplayer, int value)
{
  if (!could_meet_with_player(pplayer, aplayer)) {
    return;
  }

  handle_diplomacy_init_meeting_req(pplayer, player_number(aplayer));
  handle_diplomacy_create_clause_req(pplayer, player_number(aplayer),
                                     player_number(to_pplayer ? aplayer
                                                              : pplayer),
                                     what, value);
}

/**********************************************************************//**
  Does deep have a live operation aimed at one of aplayer's cities?

  This is deep's OWN commitment test, and it is deliberately not
  ai/default's `aplayer == target` no-mercy rule. Stock's target is the
  best war candidate by a love-bled score that is recomputed every
  turn; deep's operations layer is a force already formed and moving.
  Peace with a player whose city a wave is marching on would strand
  that wave, so the wave -- not a preference -- is what vetoes the
  proposal.
**************************************************************************/
static bool deep_op_aims_at(const struct player *pplayer,
                            const struct player *aplayer)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int oi;

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    const struct city *target;

    if (dp->ops[oi].state == DEEP_OP_NONE || dp->ops[oi].target_city <= 0) {
      continue;
    }
    target = game_city_by_number(dp->ops[oi].target_city);
    if (target != nullptr && city_owner(target) == aplayer) {
      return TRUE;
    }
  }

  return FALSE;
}

/**********************************************************************//**
  Gold-equivalent worth to `pplayer` of holding an embassy with
  `aplayer`, under a tech-leakage rule where an embassy IS the research
  discount.

  Every term is read, none is invented:

  - The rate and the denominator are the ruleset's own. Under
    TECH_LEAKAGE_EMBASSIES (common/research.c:954-981) a tech's cost
    falls by `tech_leak_pct / alive_players` percent for EACH player
    who knows it and with whom we hold an embassy. The count is per
    player and is not de-duplicated across embassies, so one new
    embassy with aplayer is worth exactly one such step on every tech
    aplayer knows and we do not.
  - The price of a step is this research's own remaining bulb cost. It
    is the post-leak figure rather than the base cost the formula uses,
    which makes this estimate CONSERVATIVE by exactly the leak already
    banked.
  - Bulbs become gold at ai/default's rate, not a new one.

  What it does not model is discounting: the techs aplayer is ahead on
  are collected over the rest of the game, not today. That is what
  embassy_price_gain is the residue for, and it is why the knob is a
  PLACEHOLDER rather than a derivation.
**************************************************************************/
static double deep_embassy_leak_worth(const struct player *pplayer,
                                      const struct player *aplayer)
{
  const struct research *mine = research_get(pplayer);
  const struct research *theirs = research_get(aplayer);
  int players = 0, ahead = 0, bulbs;

  if (game.info.tech_leakage != TECH_LEAKAGE_EMBASSIES
      || game.info.tech_leak_pct <= 0
      || mine == theirs
      || player_has_embassy(pplayer, aplayer)) {
    return 0.0;
  }

  players_iterate_alive(any) {
    players++;
  } players_iterate_alive_end;

  advance_index_iterate(A_FIRST, idx) {
    if (research_invention_state(theirs, idx) == TECH_KNOWN
        && research_invention_state(mine, idx) != TECH_KNOWN) {
      ahead++;
    }
  } advance_index_iterate_end;

  if (ahead <= 0 || players <= 0) {
    return 0.0;
  }

  bulbs = research_total_bulbs_required(mine, mine->researching, FALSE);
  if (bulbs <= 0) {
    return 0.0;
  }

  return DEEP_BULBS_TO_GOLD * bulbs * ahead
         * game.info.tech_leak_pct / (100.0 * players);
}

/**********************************************************************//**
  Deep's own gold-equivalent for one embassy clause, positive for what
  we receive and negative for what we give away.
**************************************************************************/
static double deep_embassy_clause_worth(struct player *pplayer,
                                        struct player *aplayer,
                                        const struct Clause *pclause)
{
  const struct deep_tuning *t = deep_tuning_get();

  if (pclause->from == pplayer) {
    /* We are the giver: they get the discount we would have denied
     * them. Weighted, because a rival's gain and our own are not the
     * same event -- ours is certain and immediate. */
    return -t->embassy_price_gain * t->embassy_give_gain
           * deep_embassy_leak_worth(aplayer, pplayer);
  }

  return t->embassy_price_gain * deep_embassy_leak_worth(pplayer, aplayer);
}

/**********************************************************************//**
  Would deep sign a durable peace with aplayer right now?

  Sets *love and *bar to the numbers compared, and *why to the reason
  when the answer is no. Every structural guard here is ai/default's
  own (daidiplomacy.c:395-417, :1799-1806): deep waives a LOVE
  THRESHOLD and nothing else, because love is the quantity the trap is
  made of -- best_target bleeds up to 50 a turn against a 125 bar, so a
  pair can sit permanently under a bar it would clear if it were ever
  asked on a turn the window was open.
**************************************************************************/
static bool deep_peace_wanted(struct ai_type *ait, struct player *pplayer,
                              struct player *aplayer,
                              int *love, int *bar, const char **why)
{
  const struct deep_tuning *t = deep_tuning_get();
  const struct player_diplstate *ds
    = player_diplstate_get(pplayer, aplayer);
  struct ai_dip_intel *adip = dai_diplomacy_get(ait, pplayer, aplayer);
  bool close_here;
  struct ai_plr *ai = dai_plr_data_get(ait, pplayer, &close_here);
  bool wanted = FALSE;

  *love = pplayer->ai_common.love[player_index(aplayer)];
  *bar = (int) (ai->diplomacy.req_love_for_peace
                * (1.0 - t->peace_seek_gain));
  *why = "yes";

  if (close_here) {
    dai_data_phase_finished(ait, pplayer);
  }

  if (has_handicap(pplayer, H_AWAY)) {
    *why = "away";
  } else if (!could_meet_with_player(pplayer, aplayer)) {
    *why = "nocontact";
  } else if (!pplayers_non_attack(pplayer, aplayer)) {
    /* The counterparty refuses outright while shots are still being
     * fired (:399-404); a cease-fire satisfies this. */
    *why = "shooting";
  } else if (ds->type != DS_CEASEFIRE) {
    *why = "notceasefire";
  } else if (ds->turns_left > DEEP_PEACE_WINDOW || ds->turns_left <= 0) {
    *why = "windowshut";
  } else if (adip->countdown >= 0 || adip->countdown < -1) {
    /* A war we have already committed to and told our allies about. */
    *why = "countdown";
  } else if (adip->at_war_with_ally != nullptr) {
    *why = "allyswar";
  } else if (adip->is_allied_with_enemy != nullptr
             || deep_allied_with_enemy(pplayer, aplayer) != nullptr) {
    *why = "enemyally";
  } else if (deep_op_aims_at(pplayer, aplayer)) {
    *why = "ourtarget";
  } else if (*love < *bar) {
    *why = "love";
  } else {
    wanted = TRUE;
  }

  return wanted;
}

/**********************************************************************//**
  What a non-war relationship with `aplayer` is worth to `pplayer`,
  in GOLD -- the currency every treaty clause is already denominated
  in, so a valuation and a price can be compared without a second
  exchange rate being minted between them.

  Two components, both read from live state:

  - THE TRADE AT STAKE. Active routes with them earn
    deep_route_value_with() trade a turn, and a war cancels them
    (common/traderoutes.c's own cancellation rules) -- so the stock at
    risk is that flow over deep's standing horizon. MORT is exactly
    amortize()'s annuity factor in this project, which is why the
    trade evaluator already divides by it to turn a stock into a flow;
    multiplying the other way mints nothing. A trade point is counted
    as one gold, which is CONSERVATIVE: ai/default values the bulb
    half of a trade point at 3 gold (dai_goldequiv_tech).

  - THE BORDER PRESSURE. Their cities near ours are where a land war
    would actually be fought, and deep already has the lattice that
    measures exactly that (deep_border_pressure_from(), the density
    fear aggregated per pair). Its units are danger, not gold, and
    there is no honest read anywhere in the tree for the exchange
    between them -- so gift_border_value is that rate, a PLACEHOLDER
    flagged for the tuner rather than a number picked here.

  What this is NOT: an estimate of who would win. Deep has a combat
  model and it is not cheap enough to run against every player every
  turn, and the question a gift asks is narrower -- what am I
  protecting -- than the question a war plan asks.
**************************************************************************/
static double deep_peace_worth(const struct player *pplayer,
                               const struct player *aplayer,
                               double *trade_out, double *border_out)
{
  const struct deep_tuning *t = deep_tuning_get();
  double trade = (double) deep_route_value_with(pplayer, aplayer) * MORT;
  double border = deep_border_pressure_from(pplayer, aplayer)
                  * t->gift_border_value;

  if (trade_out != nullptr) {
    *trade_out = trade;
  }
  if (border_out != nullptr) {
    *border_out = border;
  }

  return trade + border;
}

/**********************************************************************//**
  Gold this player may spend on diplomacy at all: everything above the
  reserve ai/default's own treasury guard keeps.

  STRUCTURAL, not a fraction: dai_gold_reserve() is the floor deep
  already refuses to spend below (it is what deep_balance_treasury()
  is defending), so re-using it means a gift can never be the reason a
  building gets sold. A second "fraction of treasury" knob on top of
  it would be a number chosen here, and the willingness bound below is
  already the tunable that decides how much of an affordable gift is
  actually worth making.
**************************************************************************/
static int deep_gift_budget(struct player *pplayer)
{
  int spare = pplayer->economic.gold - dai_gold_reserve(pplayer);

  return MAX(spare, 0);
}

/**********************************************************************//**
  Lever 1. On the turns the cease-fire window is open, ask.

  ai/default computes the same answer and is not allowed to act on it:
  its anti-spam gate skips the whole AI-AI branch for fc_rand(4)+3
  turns (:1814,:1824) and the window is four turns wide, so the pass
  that would propose is not running on the turns it would be allowed
  to. This pre-pass runs before that gate, on exactly those turns, and
  on no others.
**************************************************************************/
static void deep_peace_seek(struct ai_type *ait, struct player *pplayer,
                            struct player *aplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  const struct player_diplstate *ds
    = player_diplstate_get(pplayer, aplayer);
  int love = 0, bar = 0, left;
  const char *why = "";
  bool wanted;

  if (ds->type != DS_CEASEFIRE
      || ds->turns_left > DEEP_PEACE_WINDOW
      || ds->turns_left <= 0) {
    /* Not a window. Counted nowhere: the census denominator is the
     * population the lever could act on, not every pair alive. */
    return;
  }

  /* SNAPSHOT BEFORE PROPOSING. `ds` points at live state and the
   * treaty concludes synchronously inside deep_treaty_suggest(), so
   * reading turns_left afterwards reports the ARMISTICE's 16 and not
   * the window that was open when the decision was made. The first
   * probe's log said `left=16` on every successful proposal, which is
   * a measurement wrong in its own output. */
  left = ds->turns_left;

  dp->peace_windows++;
  wanted = deep_peace_wanted(ait, pplayer, aplayer, &love, &bar, &why);

  if (wanted) {
    dp->peace_proposed++;
    deep_treaty_clear(pplayer, aplayer);
    deep_treaty_suggest(pplayer, aplayer, CLAUSE_PEACE, FALSE, 0);
  }

  /* `now` is read after, on purpose: a proposal that concluded has
   * already moved the pair to DS_ARMISTICE, so this one field
   * separates "asked" from "got it" without any stored state. */
  log_base(deep_metric_log_level(),
           "deep-peaceseek turn=%d plr=\"%s\" to=\"%s\" left=%d love=%d"
           " bar=%d sent=%d now=%d why=\"%s\"",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           left, love, bar, wanted ? 1 : 0,
           (int) player_diplstate_get(pplayer, aplayer)->type, why);
}

/**********************************************************************//**
  What ai/default's own gift rule would charge to move `aplayer`'s
  love for us by `want` points, in gold, or 0 if it cannot.

  This is not a model of the recipient: it is dai_treaty_accepted()'s
  arithmetic read backwards (daidiplomacy.c:775-782). A treaty whose
  every clause is beneficial to the receiver adds

      love += MIN(balance / (cities x 10 + 1), love_incr x 150) x 10

  so the gold that buys L love is ceil(L/10) x (cities x 10 + 1),
  grossed up for the diplgoldcost the server skims off a transfer.
  Note the granularity that falls straight out of it: love arrives in
  steps of ten, and anything under one step buys nothing at all --
  which is why a gift must be sized rather than dribbled.
**************************************************************************/
static int deep_gift_price_of_love(const struct player *aplayer, int want)
{
  int steps = (want + 9) / 10;
  int per_step = city_list_size(aplayer->cities) * 10 + 1;
  int gold;

  if (want <= 0 || game.server.diplgoldcost >= 100) {
    return 0;
  }
  gold = steps * per_step;

  /* The receiver banks value x (100 - diplgoldcost) / 100 (the GOLD
   * clause, daidiplomacy.c:460-466), so what leaves our treasury is
   * larger than what lands in theirs. */
  return (gold * 100 + (99 - game.server.diplgoldcost))
         / (100 - game.server.diplgoldcost);
}

/**********************************************************************//**
  What one advance is worth to the player RECEIVING it, in the gold
  the treaty balance is kept in.

  ai/default's own price (dai_goldequiv_tech, daidiplomacy.c:158-179)
  is `3 x goal bulbs + tech_want/turn`, halved when the prereqs are
  already known. Deep reads the first term and drops the second: the
  tech_want half is the receiver's private opinion of what it wants,
  and dropping it makes this a LOWER bound on what the counterparty
  will bank -- which is the safe direction, because the whole point is
  to be sure a proposal clears.

  It is not a copy of a static function so much as the same two
  ruleset reads; the halving is stock's and is kept, because a tech
  whose prereqs are known is genuinely nearer and therefore cheaper.
**************************************************************************/
static int deep_tech_worth_to(const struct player *aplayer, Tech_type_id tech)
{
  const struct research *theirs = research_get(aplayer);
  enum tech_state state = research_invention_state(theirs, tech);
  int worth;

  if (state == TECH_KNOWN
      || !research_invention_gettable(theirs, tech,
                                      game.info.tech_trade_allow_holes)) {
    return 0;
  }
  worth = research_goal_bulbs_required(theirs, tech) * 3;
  if (state == TECH_PREREQS_KNOWN) {
    worth /= 2;
  }

  return worth;
}

/**********************************************************************//**
  May we hand `tech` to `aplayer` without arming somebody we are
  fighting?

  This is ai/default's own tech-leak veto (compute_tech_sell_price,
  daidiplomacy.c:239-258) read as a TARGETING CRITERION rather than as
  a price adjustment. Stock refuses a sale when the buyer is allied to
  a player the seller finds dangerous; deep applies the same refusal
  and adds the channel stock does not model, because stock was written
  for a ruleset family where it does not exist:

  - ALLIANCE. An ally of the receiver gets the tech outright through
    dai_share(), so a receiver allied to anyone we are at war with is
    refused, whatever they would pay.

  - EMBASSY LEAKAGE. Under tech_leakage = "Embassies" -- civ2civ3's
    setting -- every embassy an enemy holds with a player who knows a
    tech makes that tech cheaper for the enemy (common/research.c:
    954-981). So handing a tech to a player our enemy has an embassy
    with is handing a discount to our enemy, one step removed. Stock
    prices tech leakage between ALLIES and has no term for this at
    all.

  Both tests are skipped for players who already know the tech: they
  cannot learn it twice, and refusing on their account would make the
  veto tighten as the tech spread, which is backwards.
**************************************************************************/
static bool deep_tech_safe_to_give(const struct player *pplayer,
                                   const struct player *aplayer,
                                   Tech_type_id tech)
{
  bool embassies = game.info.tech_leakage == TECH_LEAKAGE_EMBASSIES;

  players_iterate_alive(eplayer) {
    if (eplayer == pplayer || eplayer == aplayer
        || research_invention_state(research_get(eplayer), tech)
           == TECH_KNOWN) {
      continue;
    }
    if (player_diplstate_get(pplayer, eplayer)->type != DS_WAR
        && !adv_is_player_dangerous((struct player *) pplayer,
                                    (struct player *) eplayer)) {
      continue;
    }
    if (pplayers_allied(aplayer, eplayer)) {
      return FALSE;
    }
    if (embassies && player_has_embassy(eplayer, aplayer)) {
      return FALSE;
    }
  } players_iterate_alive_end;

  return TRUE;
}

/**********************************************************************//**
  The cheapest advance we could safely hand `aplayer` that is worth at
  least `floor` gold to them, or A_NONE.

  Cheapest rather than best on purpose: a gift is a concession, and
  the concession that buys the same thing for less is the better one.
  `floor` exists because the love the gift buys arrives in steps of
  ten (see deep_gift_price_of_love) -- below one step a gift is a
  tech given away for nothing at all.
**************************************************************************/
static Tech_type_id deep_gift_tech_pick(const struct player *pplayer,
                                        const struct player *aplayer,
                                        int floor, int *worth_out)
{
  const struct research *mine = research_get(pplayer);
  Tech_type_id best = A_NONE;
  int best_worth = 0;

  advance_index_iterate(A_FIRST, idx) {
    int worth;

    if (research_invention_state(mine, idx) != TECH_KNOWN) {
      continue;
    }
    worth = deep_tech_worth_to(aplayer, idx);
    if (worth < floor || worth <= 0) {
      continue;
    }
    if (best != A_NONE && worth >= best_worth) {
      continue;
    }
    if (!deep_tech_safe_to_give(pplayer, aplayer, idx)) {
      continue;
    }
    best = idx;
    best_worth = worth;
  } advance_index_iterate_end;

  if (worth_out != nullptr) {
    *worth_out = best_worth;
  }

  return best;
}

/**********************************************************************//**
  The gift lever's census. One line per turn per pair deep is in a
  non-war, non-allied relationship with -- the exact population the
  two gift paths can act on -- carrying what the relationship is worth
  to us, what we could spend, how far the counterparty's love is from
  its own peace bar, and what closing that gap would cost.

  Logged before any of it is spent, and logged with the levers off, so
  the session has the distribution BEFORE it has the mechanism: the
  brief's stop condition is "recipients structurally refuse", and a
  refusal is only legible against the price that was on the table.
**************************************************************************/
static void deep_gift_probe(struct ai_type *ait, struct player *pplayer,
                            struct player *aplayer)
{
  const struct player_diplstate *ds
    = player_diplstate_get(pplayer, aplayer);
  struct ai_plr *theirs;
  bool close_here;
  double trade = 0.0, border = 0.0, worth;
  int love, bar, shortfall, gold;

  if (ds->type != DS_CEASEFIRE && ds->type != DS_ARMISTICE
      && ds->type != DS_PEACE) {
    return;
  }

  worth = deep_peace_worth(pplayer, aplayer, &trade, &border);

  /* Their love for US, and their own bar: both live AI state, and
   * both of the same class ai/default's treaty valuation already
   * reads across the table (compute_tech_sell_price() prices a tech
   * from the TAKER's research, dai_goldequiv_clause() reads the
   * counterparty's cities). This introduces no information deep did
   * not already have; W04's honest-information work owns the question
   * of whether any of it should be readable. */
  love = aplayer->ai_common.love[player_index(pplayer)];
  theirs = dai_plr_data_get(ait, aplayer, &close_here);
  bar = theirs->diplomacy.req_love_for_peace;
  if (close_here) {
    dai_data_phase_finished(ait, aplayer);
  }

  shortfall = MAX(bar - love, 0);
  gold = deep_gift_price_of_love(aplayer, shortfall);

  log_base(deep_metric_log_level(),
           "deep-gift turn=%d plr=\"%s\" to=\"%s\" ds=%d worth=%.1f"
           " trade=%.1f border=%.1f budget=%d theirlove=%d bar=%d"
           " short=%d price=%d clear=%d",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           (int) ds->type, worth, trade, border,
           deep_gift_budget(pplayer), love, bar, shortfall, gold,
           /* What the SAME shortfall costs as a treaty balance rather
            * than as love: greed() charges the square of it over ten
            * (daidiplomacy.c:125-134), which is the gold that makes a
            * refused peace clause add up. Two prices for two
            * different products, and they are not the same number. */
           shortfall * shortfall / 10);
}

/**********************************************************************//**
  Offer one advance to `aplayer`, optionally alongside a pact clause
  so the offer is legal at all, and report whether it was taken.

  ai/default refuses EVERY tech clause while `ds_after == DS_WAR`
  (dai_treaty_evaluate: an ADVANCE is not on its gift list unless the
  ruleset's tech_cost_style is Civ1|2, and neither shipped ruleset
  sets that). So a gift to a player we are still nominally at war with
  has to carry the clause that changes ds_after -- a cease-fire, which
  costs the receiver nothing (its own valuation is 0 or greed(love))
  and is exempt from the allied-with-our-enemy veto that blocks peace.

  `pact` is CLAUSE_LAST for "no pact clause needed". The pact goes on
  FIRST so the final evaluation sees the complete treaty; every
  create_clause call clears both accept flags, so an intermediate
  state can never conclude by itself.
**************************************************************************/
static bool deep_gift_send(struct ai_type *ait, struct player *pplayer,
                           struct player *aplayer, Tech_type_id tech,
                           enum clause_type pact)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  bool taken;

  if (!could_meet_with_player(pplayer, aplayer)) {
    return FALSE;
  }

  dp->gift_open_plus1 = player_number(aplayer) + 1;
  dp->gift_tech = tech;
  dp->gifts_offered++;

  deep_treaty_clear(pplayer, aplayer);
  if (pact != CLAUSE_LAST) {
    deep_treaty_suggest(pplayer, aplayer, pact, FALSE, 0);
  }
  deep_treaty_suggest(pplayer, aplayer, CLAUSE_ADVANCE, FALSE, tech);

  dp->gift_open_plus1 = 0;
  dp->gift_tech = A_NONE;

  taken = research_invention_state(research_get(aplayer), tech)
          == TECH_KNOWN;
  if (taken) {
    dp->gifts_taken++;
  }

  return taken;
}

/**********************************************************************//**
  Lever 1b. Buy back the love a partner has bled, with the currency
  deep actually holds.

  THE MEASUREMENT THAT CHOSE THE INSTRUMENT. The brief asked for
  "small gold/tech gift clauses". The gift census (commit before this
  one, 8 games over contested and lategame-80) says gold is not
  available: the treasury above ai/default's own reserve is a MEDIAN
  of 17 gold on contested and 0 on lategame-80, against a love price
  whose 90th percentile is 1085 and 1729, and the price is affordable
  on 6.1% and 1.3% of the rows with a shortfall. Deep is not a rich
  player with a diplomacy budget; it is a player that spends its gold
  the turn it arrives.

  A tech is the instrument it does hold. ai/default prices one at
  three times the bulbs it would take the receiver to get there --
  hundreds to thousands of gold, at no cost to any treasury -- and
  dai_treaty_accepted() converts a wholly-beneficial treaty into love
  at a rate this file computes exactly. So the gift is a tech, chosen
  as the CHEAPEST safe one that still buys a whole ten-love step.

  What deep pays is not gold, and the willingness bound says so: the
  gift happens when what the receiver banks is no more than
  gift_love_gain times what the relationship is worth to us. Both
  sides of that comparison are in gold and neither is invented.

  EXPLOITABILITY, stated rather than hidden: this hands real advances
  to a rival to keep them friendly, and a human who reads this can
  farm it -- keep deep's love just under its own peace bar and collect
  a tech. The structural limits are that the gift is refused when it
  would arm anyone we are fighting (deep_tech_safe_to_give), that it
  stops the moment love clears the bar, and that gift_love_gain tunes
  the whole thing to zero.
**************************************************************************/
static void deep_love_gift(struct ai_type *ait, struct player *pplayer,
                           struct player *aplayer)
{
  const struct deep_tuning *t = deep_tuning_get();
  const struct player_diplstate *ds
    = player_diplstate_get(pplayer, aplayer);
  struct ai_plr *theirs;
  bool close_here;
  double worth;
  int love, bar, step, tech_worth = 0, gain;
  Tech_type_id tech;

  if (ds->type != DS_PEACE && ds->type != DS_ARMISTICE
      && ds->type != DS_CEASEFIRE) {
    /* Durability is only a question where there is something to keep.
     * A war is BALANCE's other lever, and an alliance is already past
     * every bar this one could raise. */
    return;
  }

  love = aplayer->ai_common.love[player_index(pplayer)];
  theirs = dai_plr_data_get(ait, aplayer, &close_here);
  bar = theirs->diplomacy.req_love_for_peace;
  if (close_here) {
    dai_data_phase_finished(ait, aplayer);
  }
  if (love >= bar) {
    return;                     /* Nothing to buy. */
  }

  /* One step of love is the smallest gift that moves anything: the
   * rule is integer division by (cities x 10 + 1) and then x 10. */
  step = city_list_size(aplayer->cities) * 10 + 1;
  worth = deep_peace_worth(pplayer, aplayer, nullptr, nullptr);
  if (worth <= 0.0) {
    return;
  }

  tech = deep_gift_tech_pick(pplayer, aplayer, step, &tech_worth);
  if (tech == A_NONE || tech_worth > t->gift_love_gain * worth) {
    return;
  }

  gain = MIN(tech_worth / step, theirs->diplomacy.love_incr * 150) * 10;
  log_base(deep_metric_log_level(),
           "deep-lovegift turn=%d plr=\"%s\" to=\"%s\" ds=%d love=%d"
           " bar=%d tech=\"%s\" price=%d worth=%.1f expect=%d took=%d",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           (int) ds->type, love, bar,
           advance_rule_name(advance_by_number(tech)), tech_worth, worth,
           gain,
           deep_gift_send(ait, pplayer, aplayer, tech, CLAUSE_LAST) ? 1 : 0);
}

/**********************************************************************//**
  Lever 2, proposal side. At peace or better, ask for the embassy the
  ruleset gives away free -- and offer ours back when they lack it, so
  the counterparty has a balance to weigh rather than a demand.

  Asked at most once per diplomatic state per pair, recorded as
  state + 1 so a zeroed slot means "never asked". That is the whole
  anti-spam rule and it needs no timer: deep's own valuation of the
  clause does not change with the turn number (which is the bug it
  exists to replace), so a refusal is permanent information until the
  relationship itself changes.
**************************************************************************/
static void deep_embassy_offer(struct ai_type *ait, struct player *pplayer,
                               struct player *aplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  enum diplstate_type ds = player_diplstate_get(pplayer, aplayer)->type;
  int slot = player_index(aplayer);
  bool want_theirs, give_ours;

  /* PEACE LEVEL ONLY. Alliance and team are ai/default's: dai_share()
   * already swaps embassies there and prices giving at zero, on the
   * reasoning that an ally's research is partly one's own. Deep has no
   * evidence that reasoning is wrong and no brief to test it, and
   * charging an ally for a clause stock hands over free could only
   * REMOVE embassies from the 22.6% of pairs that end allied. The
   * population this lever exists for is the one where the clause is
   * arithmetically impossible and never proposed: peace. */
  if (ds != DS_PEACE && ds != DS_ARMISTICE) {
    return;
  }
  if (slot < 0 || slot >= MAX_NUM_PLAYER_SLOTS) {
    return;
  }

  want_theirs = !player_has_embassy(pplayer, aplayer);
  give_ours = !player_has_embassy(aplayer, pplayer);

  if (!want_theirs) {
    /* We already hold it. Giving ours away for nothing is a gift, and
     * gifts are BALANCE's business, not this session's. */
    return;
  }
  if (dp->embassy_asked[slot] == (int) ds + 1) {
    return;
  }
  dp->embassy_asked[slot] = (int) ds + 1;

  deep_treaty_clear(pplayer, aplayer);
  deep_treaty_suggest(pplayer, aplayer, CLAUSE_EMBASSY, TRUE, 0);
  if (give_ours) {
    deep_treaty_suggest(pplayer, aplayer, CLAUSE_EMBASSY, FALSE, 0);
  }
}

/**********************************************************************//**
  Lever 3. Who is running away with it?

  Computed here rather than read from adv->dipl because the stock loop
  (advdata.c:486-500) iterates players_iterate -- not the _alive form
  -- with a strict `<`, and can therefore name a player who is already
  dead. Ties resolve to nobody: three players level on techs is not a
  runaway, and a signal that fires on a tie is a signal that fires
  always.
**************************************************************************/
void deep_leaders_refresh(struct ai_type *ait, struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  bool close_here;
  struct ai_plr *ai;
  int tech_best = -1, prod_best = -1;
  int tech_id = -1, prod_id = -1;
  int tech_ties = 0, prod_ties = 0;

  if (dp->leaders_turn == game.info.turn) {
    return;
  }
  dp->leaders_turn = game.info.turn;

  players_iterate_alive(aplayer) {
    if (is_barbarian(aplayer)) {
      continue;
    }
    if (aplayer->score.techs > tech_best) {
      tech_best = aplayer->score.techs;
      tech_id = player_number(aplayer);
      tech_ties = 1;
    } else if (aplayer->score.techs == tech_best) {
      tech_ties++;
    }
    if (aplayer->score.mfg > prod_best) {
      prod_best = aplayer->score.mfg;
      prod_id = player_number(aplayer);
      prod_ties = 1;
    } else if (aplayer->score.mfg == prod_best) {
      prod_ties++;
    }
  } players_iterate_alive_end;

  dp->tech_leader_id = (tech_ties == 1) ? tech_id : -1;
  dp->prod_leader_id = (prod_ties == 1) ? prod_id : -1;

  ai = dai_plr_data_get(ait, pplayer, &close_here);
  dp->wincap_seen = (ai->diplomacy.strategy == WIN_CAPITAL);
  if (close_here) {
    dai_data_phase_finished(ait, pplayer);
  }

  log_base(deep_metric_log_level(),
           "deep-leaders turn=%d plr=\"%s\" tech=%d prod=%d techscore=%d"
           " prodscore=%d wincap=%d live=%d applied=%d spared=%d"
           " loved=%d",
           game.info.turn, player_name(pplayer), dp->tech_leader_id,
           dp->prod_leader_id, tech_best, prod_best,
           dp->wincap_seen ? 1 : 0,
           (dp->tech_leader_id >= 0 || dp->prod_leader_id >= 0) ? 1 : 0,
           dp->leader_bias_applied, dp->trade_partner_spared,
           dp->trade_love_applied);
}

/**********************************************************************//**
  Is aplayer the runaway on either axis, from pplayer's point of view?
  Never true of pplayer itself: leading is not a reason to attack
  oneself, and the bias exists to point a wave at somebody else.
**************************************************************************/
bool deep_player_is_leader(struct ai_type *ait, struct player *pplayer,
                           const struct player *aplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int id;

  if (aplayer == pplayer) {
    return FALSE;
  }
  deep_leaders_refresh(ait, pplayer);
  id = player_number(aplayer);

  return id == dp->tech_leader_id || id == dp->prod_leader_id;
}

/**********************************************************************//**
  Can a treaty clause deliver an embassy with aplayer?

  The composition point between deep's two embassy channels. With the
  knobs shut this is always FALSE and the diplomat want is exactly what
  it has always been.
**************************************************************************/
bool deep_embassy_by_treaty(struct ai_type *ait, const struct player *pplayer,
                            const struct player *aplayer)
{
  enum diplstate_type ds = player_diplstate_get(pplayer, aplayer)->type;

  if (deep_tuning_get()->embassy_price_gain <= 0.0
      || !is_ai(aplayer)) {
    /* A human counterparty is under no obligation to answer a clause,
     * so the diplomat keeps owning that case. */
    return FALSE;
  }

  return ds == DS_PEACE || ds == DS_ARMISTICE;
}

/**********************************************************************//**
  Deep's own treaty ruling, for the two treaty shapes deep fully
  understands. Returns FALSE for everything else, and ai/default then
  evaluates the treaty exactly as it always has.

  The reason for shape-matching rather than adjusting a balance:
  dai_treaty_evaluate() sums its clauses privately and only publishes
  accept-or-not, so there is no total for deep to add a delta to. A
  treaty whose every clause deep prices itself is one deep can total.
**************************************************************************/
bool deep_treaty_evaluate(struct ai_type *ait, struct player *pplayer,
                          struct player *aplayer, struct treaty *ptreaty)
{
  const struct deep_tuning *t = deep_tuning_get();
  struct deep_plr *dp = deep_plr_data(pplayer);
  int n = 0, peace = 0, embassy = 0;
  double balance = 0.0;

  /* Our own offer, mid-construction. ai/default's valuation of GIVING
   * a tech away is a refusal by design (compute_tech_sell_price is
   * subtracted, and its is_dangerous flag returns -BIG_NUMBER), so
   * without this branch deep would veto every gift it just decided to
   * make. The recognition is exact rather than permissive: the only
   * clauses allowed are the pact clause deep attached and the single
   * advance it chose, all of them from deep's own side. Anything else
   * in the treaty means this is not the offer deep is making, and it
   * goes to ai/default like everything else. */
  if (dp->gift_open_plus1 == player_number(aplayer) + 1) {
    bool mine = TRUE;
    int gifts = 0;

    clause_list_iterate(ptreaty->clauses, pclause) {
      if (pclause->type == CLAUSE_ADVANCE && pclause->from == pplayer
          && pclause->value == dp->gift_tech) {
        gifts++;
      } else if (pclause->type != CLAUSE_CEASEFIRE
                 && pclause->type != CLAUSE_PEACE) {
        mine = FALSE;
      }
    } clause_list_iterate_end;

    if (mine && gifts == 1) {
      handle_diplomacy_accept_treaty_req(pplayer, player_number(aplayer));
      return TRUE;
    }
    return FALSE;
  }

  clause_list_iterate(ptreaty->clauses, pclause) {
    n++;
    if (pclause->type == CLAUSE_PEACE) {
      peace++;
    } else if (pclause->type == CLAUSE_EMBASSY) {
      embassy++;
    }
  } clause_list_iterate_end;

  if (n == 1 && peace == 1 && t->peace_seek_gain > 0.0) {
    int love, bar;
    const char *why;

    if (deep_peace_wanted(ait, pplayer, aplayer, &love, &bar, &why)) {
      dp->peace_taken++;
      log_base(deep_metric_log_level(),
               "deep-peacetake turn=%d plr=\"%s\" with=\"%s\" love=%d"
               " bar=%d accept=1",
               game.info.turn, player_name(pplayer), player_name(aplayer),
               love, bar);
      handle_diplomacy_accept_treaty_req(pplayer, player_number(aplayer));
      return TRUE;
    }
    /* Deep does not want it on deep's terms; ai/default may still want
     * it on its own, and is left free to say so. */
    return FALSE;
  }

  if (n > 0 && embassy == n && t->embassy_price_gain > 0.0
      && deep_embassy_by_treaty(ait, pplayer, aplayer)) {
    clause_list_iterate(ptreaty->clauses, pclause) {
      double worth = deep_embassy_clause_worth(pplayer, aplayer, pclause);

      balance += worth;
      dp->embassy_priced++;
      log_base(deep_metric_log_level(),
               "deep-embassy turn=%d plr=\"%s\" with=\"%s\" give=%d"
               " worth=%.1f accept=%d",
               game.info.turn, player_name(pplayer), player_name(aplayer),
               pclause->from == pplayer ? 1 : 0, worth,
               balance >= 0.0 ? 1 : 0);
    } clause_list_iterate_end;

    if (balance >= 0.0) {
      dp->embassy_cleared++;
      handle_diplomacy_accept_treaty_req(pplayer, player_number(aplayer));
    }
    return TRUE;
  }

  return FALSE;
}

/**********************************************************************//**
  The pre-pass itself.
**************************************************************************/
void deep_diplomacy_prepass(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *t = deep_tuning_get();

  /* Always: the signals are read and logged whether or not anything
   * consumes them. A free measurement of a thing nobody reads is the
   * cheapest evidence there is. */
  deep_leaders_refresh(ait, pplayer);

  if (!is_ai(pplayer) || is_barbarian(pplayer)
      || has_handicap(pplayer, H_AWAY)) {
    return;
  }
  if (t->peace_seek_gain <= 0.0 && t->embassy_price_gain <= 0.0
      && t->gift_border_value <= 0.0) {
    return;
  }

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || is_barbarian(aplayer)) {
      continue;
    }
    if (t->peace_seek_gain > 0.0) {
      deep_peace_seek(ait, pplayer, aplayer);
    }
    if (t->gift_border_value > 0.0 && is_ai(aplayer)
        && !pplayers_allied(pplayer, aplayer)) {
      deep_gift_probe(ait, pplayer, aplayer);
    }
    if (t->gift_love_gain > 0.0 && is_ai(aplayer)
        && !pplayers_allied(pplayer, aplayer)) {
      deep_love_gift(ait, pplayer, aplayer);
    }
    if (t->embassy_price_gain > 0.0) {
      deep_embassy_offer(ait, pplayer, aplayer);
    }
  } players_iterate_alive_end;
}
