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
  THE FENCE. Is `aplayer` an AI that is not running ai/deep, with
  diplo_deep_only_pacts up?

  "Fellow deep" is the ai_type pointer comparison the module already
  uses to recognise itself (deepsettler.c:2313). No new state, no name
  matching, and it survives save/load because `player->ai` does.

  Humans are exempt by construction: `is_ai()` is the first thing
  asked. The fence is a policy about which software deep shares its
  cooperative dividend with, and declining peace to a human would only
  be deep playing worse against the one opponent that could be
  surprised by it.
**************************************************************************/
static bool deep_fenced_partner(const struct player *pplayer,
                                const struct player *aplayer)
{
  return deep_tuning_get()->diplo_deep_only_pacts > 0
         && is_ai(aplayer)
         && aplayer->ai != pplayer->ai;
}

/**********************************************************************//**
  Has the counterparty already signed the treaty deep is refusing?

  A treaty executes when BOTH accept flags are set (diplhand.c:314), so
  a refusal taken while the other side has already accepted is a pact
  that only deep's answer is standing between.
**************************************************************************/
static bool deep_treaty_other_signed(const struct player *pplayer,
                                     const struct treaty *ptreaty)
{
  return ptreaty->plr0 == pplayer ? ptreaty->accept1 : ptreaty->accept0;
}

/**********************************************************************//**
  Record one fence refusal.

  `would` is the honesty field, and it is defined so that it can never
  overclaim:

     1  deep's OWN valuation says it would have signed. The fence
        certainly intercepted a pact.
     0  deep's own valuation says no.
    -1  deep has no valuation for this treaty shape at all.

  A 0 or a -1 is NOT evidence the fence changed nothing. Without the
  fence those treaties fall through to ai/default, and ai/default's
  verdict cannot be read without letting it act: dai_treaty_evaluate()
  publishes only by calling handle_diplomacy_accept_treaty_req(), and
  a treaty the counterparty has already signed EXECUTES inside that
  call. There is no way to ask and then decline.

  So `live` carries the model-free half of the same question -- the
  counterparty had already accepted -- and the pair reads: `would=1`
  counts certain interceptions, `live=1` counts treaties that were one
  signature from concluding.
**************************************************************************/
static void deep_fence_log(struct player *pplayer, struct player *aplayer,
                           const char *side, const char *level,
                           int would, bool live)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  dp->fence_refused++;
  if (would > 0) {
    dp->fence_wouldhave++;
  }

  log_base(deep_metric_log_level(),
           "deep-fence turn=%d plr=\"%s\" to=\"%s\" side=%s level=%s"
           " ds=%d would=%d live=%d refused=%d wouldhave=%d",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           side, level,
           (int) player_diplstate_get(pplayer, aplayer)->type,
           would, live ? 1 : 0, dp->fence_refused, dp->fence_wouldhave);
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

  if (wanted && deep_fenced_partner(pplayer, aplayer)) {
    /* The proposal deep decided to make, and did not. `would` is
     * exact on this path: deep_peace_wanted() IS the decision the
     * fence just overrode, so every propose-side line counts a real
     * interception. */
    deep_fence_log(pplayer, aplayer, "propose", "peace", 1, FALSE);
    wanted = FALSE;
    why = "fenced";
  }

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
  WHICH RELATIONSHIP A GIFT WOULD SERVE.

  CAMPAIGN-v4 section 11.4 is why this function exists. The love gift
  measured as the largest single-lever self-play move in the project
  (+8.28 contested, +8.92 lategame-80) and the mixed head-to-head said
  it was a TRANSFER: deep +1.93 (t 0.72), counterparty +5.26 (t 2.15),
  deep's margin over ai/default -3.34. The human's fold criterion is
  that positive sum is not the defect -- "i don't mind multiple folks
  getting better, but it _is_ more fun to get a wash as an alliance
  benefit". Surplus INSIDE the alliance is a feature; surplus that
  subsidises a rival is the failure mode that was measured.

  So the gift needs to know who it is talking to, and every criterion
  below is state deep already reads for other decisions:

  - ALLY. The alliance and its CANDIDATES. Either the pact itself, or
    a shared enemy (someone we are both at war with), or a common ally.
    The candidates are the population this lever can actually act on:
    an existing ally is past every bar the love gift can raise (its
    own cap check refuses at req_love_for_alliance - 1, and the
    pre-pass never calls it for an allied pair), so "surplus lands
    inside the alliance" here means the players who would fight beside
    us, financed while they still can be.

  - INSTRUMENT. Not a friend, but their quiet is worth buying anyway:
    we are ALREADY at war with somebody else, and this player presses
    our border by the same lattice deep_top_menacer() ranks menacers
    with. That is the second front deep cannot afford, priced rather
    than assumed. It is the ONLY reason a non-friend is worth a
    concession, and it is behind its own knob so the fold can price it
    separately from friendship or turn it off entirely.

  - RIVAL. Everyone else -- including anyone a deep operation is
    already marching on (a wave in flight is what vetoes the peace
    proposal in deep_peace_wanted(), and it vetoes friendship here for
    the same reason) and anyone allied to a player we are at war with,
    which is also the counterparty the tech-leak veto refuses.

  The classes are ORDERED and exclusive: op target and enemy-ally
  disqualify before anything can promote, so a player who both shares
  an enemy with us and shields another of our enemies is a rival.
**************************************************************************/
enum deep_gift_rel {
  DEEP_GIFT_ALLY = 0,           /* The alliance and its candidates */
  DEEP_GIFT_INSTRUMENT,         /* A front we would rather not open */
  DEEP_GIFT_RIVAL               /* Everyone else */
};

static enum deep_gift_rel deep_gift_rel_of(const struct player *pplayer,
                                           const struct player *aplayer)
{
  bool war_elsewhere = FALSE, shared_enemy = FALSE, common_ally = FALSE;

  if (deep_op_aims_at(pplayer, aplayer)) {
    return DEEP_GIFT_RIVAL;
  }
  if (pplayers_allied(pplayer, aplayer)) {
    return DEEP_GIFT_ALLY;
  }
  if (deep_allied_with_enemy(pplayer, aplayer) != nullptr) {
    return DEEP_GIFT_RIVAL;
  }

  players_iterate_alive(third) {
    if (third == pplayer || third == aplayer || is_barbarian(third)) {
      continue;
    }
    if (player_diplstate_get(pplayer, third)->type == DS_WAR) {
      war_elsewhere = TRUE;
      if (player_diplstate_get(aplayer, third)->type == DS_WAR) {
        shared_enemy = TRUE;
      }
    }
    if (pplayers_allied(pplayer, third) && pplayers_allied(aplayer, third)) {
      common_ally = TRUE;
    }
  } players_iterate_alive_end;

  if (shared_enemy || common_ally) {
    return DEEP_GIFT_ALLY;
  }
  if (war_elsewhere && deep_border_pressure_from(pplayer, aplayer) > 0.0) {
    return DEEP_GIFT_INSTRUMENT;
  }

  return DEEP_GIFT_RIVAL;
}

/**********************************************************************//**
  What the willingness bound is multiplied by for this class.

  One knob per class rather than one gate, because the classes differ
  in degree as well as in kind: a would-be ally is worth paying MORE
  for than the flat bound, which a bool could not express. 1.0
  everywhere is today's bound for everyone.
**************************************************************************/
static double deep_gift_rel_gain(const struct deep_tuning *t,
                                 enum deep_gift_rel rel)
{
  switch (rel) {
  case DEEP_GIFT_ALLY:
    return t->gift_ally_gain;
  case DEEP_GIFT_INSTRUMENT:
    return t->gift_instrument_gain;
  case DEEP_GIFT_RIVAL:
    break;
  }

  return t->gift_rival_gain;
}

/**********************************************************************//**
  The class as a log word. Bare words rather than an integer because
  the knob census reads these lines with a field predicate and a tag
  that has to be greppable by hand as well.
**************************************************************************/
static const char *deep_gift_rel_name(enum deep_gift_rel rel)
{
  switch (rel) {
  case DEEP_GIFT_ALLY:
    return "ally";
  case DEEP_GIFT_INSTRUMENT:
    return "instr";
  case DEEP_GIFT_RIVAL:
    break;
  }

  return "rival";
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
           "deep-gift turn=%d plr=\"%s\" to=\"%s\" ds=%d rel=%s worth=%.1f"
           " trade=%.1f border=%.1f budget=%d theirlove=%d bar=%d"
           " short=%d price=%d clear=%d",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           (int) ds->type, deep_gift_rel_name(deep_gift_rel_of(pplayer,
                                                               aplayer)),
           worth, trade, border,
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

  THE ALLIANCE CLAMP, structural, exactly deep_trade_route_love()'s
  (deeptrade.c). Bought love may carry a pair to PEACE and must never,
  on its own, carry it into an ALLIANCE: the ceiling is the
  counterparty's own req_love_for_alliance minus one, which varies by
  difficulty and is theirs, not ours. Love raised past that bar by
  anything else is left alone -- this only declines to push.

  Where trade love clamps the ADDITION (add = MIN(add, headroom)), a
  gift is ATOMIC: one advance, one indivisible love step, take it or
  leave it. So the analogue for an atomic action is to DECLINE when it
  would overshoot, and the prediction it declines on is exact --
  dai_treaty_accepted() computes total_balance / (cities * 10 + 1),
  clamps at love_incr * 150 and multiplies by ten
  (daidiplomacy.c:777-783), and on this path the treaty is the single
  tech clause, so `gain` below is that arithmetic and not an estimate.
  BALANCE measured a median gift of +300 expected love on lategame-80
  against a 250 bar, so the clamp is load-bearing and not decorative:
  every one of that cell's gifts had the headroom to jump the pair
  straight into alliance range.

  WHO THE SURPLUS IS FOR (GIFT-REFOLD, CAMPAIGN-v4 section 11.4). The
  bound is multiplied by the class of the relationship the gift would
  serve, so the same mechanism can pay a would-be ally more than it
  pays a rival -- or refuse a rival outright at gift_rival_gain = 0.0.
  The mixed head-to-head is what may move those knobs off 1.0; nothing
  here decides it.

  Note which population the ALLY class actually reaches. An existing
  ally never arrives at this function at all -- the pre-pass skips
  allied pairs and the state check above returns for DS_ALLIANCE --
  because bought love cannot raise a bar the alliance is already past.
  So "surplus inside the alliance" means the alliance's CANDIDATES:
  the players who share our enemies today and would fight beside us
  tomorrow, financed while financing them still buys something.
**************************************************************************/
static void deep_love_gift(struct ai_type *ait, struct player *pplayer,
                           struct player *aplayer)
{
  const struct deep_tuning *t = deep_tuning_get();
  const struct player_diplstate *ds
    = player_diplstate_get(pplayer, aplayer);
  struct ai_plr *theirs;
  struct deep_plr *dp = deep_plr_data(pplayer);
  enum deep_gift_rel rel;
  bool close_here, capped, refused = FALSE;
  double worth, bound, relbound, relgain;
  int love, bar, cap, step, tech_worth = 0, gain;
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
  cap = theirs->diplomacy.req_love_for_alliance - 1;
  if (close_here) {
    dai_data_phase_finished(ait, aplayer);
  }
  if (love >= bar) {
    return;                     /* Nothing to buy. */
  }
  if (love >= cap) {
    return;                     /* Already at the ceiling. */
  }

  /* One step of love is the smallest gift that moves anything: the
   * rule is integer division by (cities x 10 + 1) and then x 10. */
  step = city_list_size(aplayer->cities) * 10 + 1;
  worth = deep_peace_worth(pplayer, aplayer, nullptr, nullptr);
  if (worth <= 0.0) {
    return;
  }

  tech = deep_gift_tech_pick(pplayer, aplayer, step, &tech_worth);
  if (tech == A_NONE) {
    return;
  }

  /* Two bounds: what the gift would cost us against what the
   * relationship is worth (BALANCE's, unchanged), and the same bound
   * repriced for the class. A tech over BOTH is simply unaffordable
   * and returns silently as it always did; a tech under the first and
   * over the second is a gift the REFOLD refused, and that one is
   * logged, because a redirection nobody can count is indistinguishable
   * from a mechanism that never fired. */
  /* THE SEAM A VISION GIVE-LEG WOULD PLUG INTO, marked and NOT taken.
   * findings/omniscience-flag-breakage.md item (b) says giving sight is
   * an intel concession -- they see your gaps, your musters, your
   * undefended coast -- and that it "belongs in the gift/love
   * currency", which is this bound. The shape it would take is
   * arithmetic, not architecture: `tech_worth` is the whole of what a
   * gift costs us today, so a give-leg is a second term added to it
   * before the comparison, behind its own knob and its own class
   * price. It is deliberately not written here: vision is worth
   * nothing while deep is omniscient, so pricing it now would be
   * pricing a good nobody consumes. It belongs to the honest-info
   * epoch, alongside items (a), (c) and (d). */
  bound = t->gift_love_gain * worth;
  rel = deep_gift_rel_of(pplayer, aplayer);
  relgain = deep_gift_rel_gain(t, rel);
  relbound = bound * relgain;
  if (tech_worth > relbound) {
    if (tech_worth > bound) {
      return;
    }
    refused = TRUE;
    dp->gifts_offclass++;
  }

  gain = MIN(tech_worth / step, theirs->diplomacy.love_incr * 150) * 10;

  /* The clamp. Logged either way, and with the ceiling beside the
   * prediction, because a decline nobody can count is indistinguishable
   * from a mechanism that never fired. */
  capped = !refused && love + gain > cap;
  if (capped) {
    dp->gifts_capped++;
  }
  log_base(deep_metric_log_level(),
           "deep-lovegift turn=%d plr=\"%s\" to=\"%s\" ds=%d rel=%s"
           " relgain=%.2f love=%d bar=%d cap=%d tech=\"%s\" price=%d"
           " worth=%.1f bound=%.1f expect=%d capped=%d refused=%d"
           " offclass=%d took=%d",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           (int) ds->type, deep_gift_rel_name(rel), relgain, love, bar, cap,
           advance_rule_name(advance_by_number(tech)), tech_worth, worth,
           relbound, gain, capped ? 1 : 0, refused ? 1 : 0,
           dp->gifts_offclass,
           (capped || refused)
           ? 0
           : (deep_gift_send(ait, pplayer, aplayer, tech, CLAUSE_LAST)
              ? 1 : 0));
}

/**********************************************************************//**
  The best attack, and the best defence, a player could put in the
  field today: the maximum over the unit types they can currently
  build. Two numbers, one scan.

  Deliberately the RULESET's raw strengths rather than deep's combat
  model. The question here is "can this player's army threaten that
  player's cities at all", which is a capability question about unit
  types, and running the odds model over two foreign empires every
  turn would cost far more than the answer is worth.
**************************************************************************/
static void deep_best_strengths(const struct player *pplayer,
                                int *att, int *def)
{
  *att = 0;
  *def = 0;
  unit_type_iterate(ut) {
    if (!can_player_build_unit_now(pplayer, ut, RPT_CERTAIN)) {
      continue;
    }
    *att = MAX(*att, ut->attack_strength);
    *def = MAX(*def, ut->defense_strength);
  } unit_type_iterate_end;
}

/**********************************************************************//**
  The attack strength `tech` would unlock for `aplayer`: the strongest
  attacker that needs this advance and that they could otherwise
  build.

  This is CONSEQ's defender/attacker unlock scan pointed at somebody
  else's position -- the same shape, run on a third party. It reads
  is_tech_req_for_utype() plus can_player_build_unit_later(), so a
  unit gated on a second tech they do not have is correctly not
  counted as unlocked by this one.
**************************************************************************/
static int deep_tech_unlocks_attack(const struct player *aplayer,
                                    Tech_type_id tech)
{
  struct advance *padv = advance_by_number(tech);
  int best = 0;

  if (padv == nullptr) {
    return 0;
  }
  unit_type_iterate(ut) {
    if (can_player_build_unit_now(aplayer, ut, RPT_CERTAIN)
        || !can_player_build_unit_later(aplayer, ut)
        || !is_tech_req_for_utype(ut, padv)) {
      continue;
    }
    best = MAX(best, ut->attack_strength);
  } unit_type_iterate_end;

  return best;
}

/**********************************************************************//**
  Who presses hardest on us among the players we are actually at war
  with, and how hard.

  The ranking is the density lattice's, not a new one: border pressure
  is what deep already believes a neighbour costs it, and the player
  at the top of that list is the one a second front would relieve.
  Returns nullptr when nobody is at war with us -- in which case there
  is nothing for a proxy to be a proxy against, and the whole lever
  has no population that turn.
**************************************************************************/
static struct player *deep_top_menacer(const struct player *pplayer,
                                       double *pressure)
{
  struct player *worst = nullptr;
  double best = 0.0;

  *pressure = 0.0;
  players_iterate_alive(aplayer) {
    double p;

    if (aplayer == pplayer || is_barbarian(aplayer)
        || player_diplstate_get(pplayer, aplayer)->type != DS_WAR) {
      continue;
    }
    p = deep_border_pressure_from(pplayer, aplayer);
    if (p > best) {
      best = p;
      worst = aplayer;
    }
  } players_iterate_alive_end;

  *pressure = best;

  return worst;
}

/**********************************************************************//**
  Could arming `aplayer` come back at US?

  The reachability law the fear lattice already applies to threats,
  applied to a gift: a player whose cities share no landmass with any
  of ours and who floats no sea transport cannot bring a stronger army
  to us, so making them stronger does not raise our own density fear.
  The moment either half stops being true they are a neighbour, not a
  distant power, and the lever refuses them.
**************************************************************************/
static bool deep_is_distant(const struct player *pplayer,
                            const struct player *aplayer)
{
  city_list_iterate(aplayer->cities, acity) {
    Continent_id c = tile_continent(city_tile(acity));

    city_list_iterate(pplayer->cities, pcity) {
      if (tile_continent(city_tile(pcity)) == c) {
        return FALSE;
      }
    } city_list_iterate_end;
  } city_list_iterate_end;

  unit_list_iterate(aplayer->units, aunit) {
    const struct unit_type *at = unit_type_get(aunit);

    if (at->transport_capacity > 0 && deep_utype_is_sea(at)) {
      return FALSE;
    }
  } unit_list_iterate_end;

  return TRUE;
}

/**********************************************************************//**
  Lever 2. Arm a distant power that is already in the way of the
  player menacing us.

  The whole selection is readable state and every criterion is a
  criterion the code already had, pointed somewhere new:

  - THE MENACER is the top of deep's own border-pressure ranking among
    players at war with us.
  - THE PROXY is at war with, or bordering, that menacer. Both from
    diplstate and from the same lattice.
  - THE PROXY IS DISTANT from us by the fear lattice's own
    reachability law, so what we hand over cannot be brought back
    against us.
  - THE GIFT IS LEAK-SAFE by ai/default's tech-leak veto INVERTED into
    a targeting test: no alliance and, under civ2civ3's Embassies
    leakage, no embassy that would carry the advance to the menacer.
  - THE TECH is the one that most shifts THEIR balance: the strongest
    attacker it unlocks for them, and the arming stops entirely once
    their best attacker already beats the menacer's best defender.
    That stopping rule is why this cannot empty the tech tree into a
    third party -- it is satisfied, not budgeted.

  A tech clause is refused outright while ds_after is DS_WAR, so a
  proxy we have never made terms with is offered a cease-fire in the
  same treaty. That is not a concession: a cease-fire with a player on
  the far side of the world costs nothing and is exactly the state in
  which the gift becomes legal.

  ETHICS OF PLAY, said plainly because a human opponent deserves to
  read it in the changelog: with this switched on, deep will arm your
  rivals to keep you busy. It never arms someone who can reach you
  through an alliance or an embassy with the player it is trying to
  distract, and it never arms someone who could then reach deep -- but
  the intent is exactly what it looks like.
**************************************************************************/
static void deep_proxy_arm(struct ai_type *ait, struct player *pplayer,
                           struct player *menacer, double pressure)
{
  const struct deep_tuning *t = deep_tuning_get();
  struct deep_plr *dp = deep_plr_data(pplayer);
  const struct research *mine = research_get(pplayer);
  double budget = pressure * t->gift_border_value * t->proxy_arm_gain;
  int mena_att, mena_def;

  deep_best_strengths(menacer, &mena_att, &mena_def);

  players_iterate_alive(cand) {
    Tech_type_id best_tech = A_NONE;
    int best_att = 0, best_worth = 0;
    int cand_att, cand_def, love, need;
    /* The best offer that EXISTS ignoring the cease-fire's price, so a
     * candidate deep declines is reported with the size of the gap
     * rather than as a silent continue. Four levers in this project
     * have measured as "no effect" while never executing; a lever
     * that structurally cannot execute must say so in its own log. */
    int reach_worth = 0, reach_att = 0;

    if (cand == pplayer || cand == menacer || is_barbarian(cand)
        || !is_ai(cand)
        || pplayers_allied(cand, menacer)
        || pplayers_allied(pplayer, cand)
        || !could_meet_with_player(pplayer, cand)) {
      continue;
    }
    /* In the menacer's way already, by war or by geography. */
    if (player_diplstate_get(cand, menacer)->type != DS_WAR
        && deep_border_pressure_from(menacer, cand) <= 0.0) {
      continue;
    }
    if (!deep_is_distant(pplayer, cand)) {
      continue;
    }
    deep_best_strengths(cand, &cand_att, &cand_def);
    if (cand_att >= mena_def) {
      /* Already able to crack them. Arming further buys nothing this
       * lever can claim, and the stopping rule is the reason the
       * lever cannot drain the tech tree. */
      continue;
    }

    /* WHAT THE CEASE-FIRE COSTS THEM, and therefore what the tech has
     * to be worth before the treaty adds up at all. MEASURED, not
     * assumed: the first probe of this lever offered 47 cease-fire-
     * plus-tech treaties over two games and had 47 of them refused,
     * because a cease-fire clause is priced at greed(their love for
     * us) -- minus the square of the shortfall over ten
     * (daidiplomacy.c:125-134, :437-450) -- and an early advance is
     * worth a few hundred gold against a four-figure grudge.
     *
     * The square is used at FULL strength even though ai/default
     * charges nothing at all while the pair has been in contact for
     * under fifteen turns: deep does not need to know which side of
     * that line a pair is on, because requiring the larger number can
     * only decline an offer that would have been accepted, never
     * force through one that would not. */
    love = cand->ai_common.love[player_index(pplayer)];
    need = (player_diplstate_get(pplayer, cand)->type == DS_WAR
            && love < 0) ? love * love / 10 : 0;
    if (need > budget) {
      continue;
    }

    advance_index_iterate(A_FIRST, idx) {
      int unlocked, worth;

      if (research_invention_state(mine, idx) != TECH_KNOWN) {
        continue;
      }
      unlocked = deep_tech_unlocks_attack(cand, idx);
      if (unlocked <= cand_att) {
        continue;
      }
      worth = deep_tech_worth_to(cand, idx);
      if (worth <= 0 || worth > budget) {
        continue;
      }
      if (!deep_tech_safe_to_give(pplayer, cand, idx)) {
        continue;
      }
      /* Recorded AFTER the safety veto, so `reach` means "the best
       * offer that actually exists" and the only thing separating it
       * from an offer made is the cease-fire's price. A census that
       * counted techs the veto refuses would answer a question nobody
       * asked. */
      if (worth > reach_worth) {
        reach_worth = worth;
        reach_att = unlocked;
      }
      if (worth < need) {
        continue;               /* Would not clear the cease-fire. */
      }
      /* The strongest unlock wins; among equals, the cheapest
       * concession does. A gift is a concession and the one that buys
       * the same front for less is the better one. */
      if (best_tech != A_NONE
          && (unlocked < best_att
              || (unlocked == best_att && worth >= best_worth))) {
        continue;
      }
      best_att = unlocked;
      best_tech = idx;
      best_worth = worth;
    } advance_index_iterate_end;

    {
      enum diplstate_type ds = player_diplstate_get(pplayer, cand)->type;
      bool took = best_tech != A_NONE
                  && deep_gift_send(ait, pplayer, cand, best_tech,
                                    ds == DS_WAR ? CLAUSE_CEASEFIRE
                                                 : CLAUSE_LAST);

      if (took) {
        dp->proxies_armed++;
      }
      log_base(deep_metric_log_level(),
               "deep-proxy turn=%d plr=\"%s\" menacer=\"%s\" press=%.1f"
               " proxy=\"%s\" ds=%d tech=\"%s\" price=%d budget=%.0f"
               " candatt=%d menadef=%d unlock=%d need=%d love=%d"
               " reach=%d reachatt=%d took=%d armed=%d",
               game.info.turn, player_name(pplayer), player_name(menacer),
               pressure, player_name(cand), (int) ds,
               best_tech == A_NONE
                 ? "none"
                 : advance_rule_name(advance_by_number(best_tech)),
               best_worth, budget, cand_att, mena_def, best_att, need,
               love, reach_worth, reach_att, took ? 1 : 0,
               dp->proxies_armed);
    }
  } players_iterate_alive_end;
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

  /* THE FENCE, and it is first because it is a veto: nothing below it
   * may sign, and claiming the treaty (returning TRUE) is what keeps
   * ai/default from ruling on it either. Every clause bundled with the
   * pact -- embassy, shared vision, gold -- dies with the treaty it
   * rode in on, which is exactly the brief's "and what rides on them".
   *
   * CLAUSE_CEASEFIRE is deliberately not looked for. A cease-fire is
   * not a clinch, PEACE's cease-fire-trap pricing is the instrument
   * the whole diplomacy line was built on, and closing it here would
   * take that instrument away for nothing.
   *
   * Existing treaties are untouched: this path sees only agreements
   * being made. */
  if (deep_fenced_partner(pplayer, aplayer)) {
    bool ally = FALSE, pact = FALSE;
    int nc = 0, npeace = 0;

    clause_list_iterate(ptreaty->clauses, pclause) {
      nc++;
      if (pclause->type == CLAUSE_ALLIANCE) {
        ally = TRUE;
        pact = TRUE;
      } else if (pclause->type == CLAUSE_PEACE) {
        npeace++;
        pact = TRUE;
      }
    } clause_list_iterate_end;

    if (pact) {
      int love, bar, would = -1;
      const char *why;

      /* Deep prices exactly one pact shape itself -- a lone peace
       * clause, and only while peace_seek_gain is up -- so that is the
       * one shape where the honesty field can be exact. Anything else
       * would have gone to ai/default and is logged as unknown rather
       * than guessed. */
      if (nc == 1 && npeace == 1 && !ally && t->peace_seek_gain > 0.0) {
        would = deep_peace_wanted(ait, pplayer, aplayer, &love, &bar, &why)
                ? 1 : 0;
      }
      deep_fence_log(pplayer, aplayer, "accept",
                     ally ? "alliance" : "peace", would,
                     deep_treaty_other_signed(pplayer, ptreaty));

      return TRUE;
    }
  }

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
      && t->gift_border_value <= 0.0 && t->gift_love_gain <= 0.0
      && t->proxy_arm_gain <= 0.0) {
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

  /* Lever 2 is a per-PLAYER decision, not a per-pair one: it names one
   * menacer first and only then looks for somebody already standing in
   * that player's way. */
  if (t->proxy_arm_gain > 0.0) {
    double pressure = 0.0;
    struct player *menacer = deep_top_menacer(pplayer, &pressure);

    if (menacer != nullptr && pressure > 0.0) {
      deep_proxy_arm(ait, pplayer, menacer, pressure);
    }
  }
}
