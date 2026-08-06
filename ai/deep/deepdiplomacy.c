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

/* ai */
#include "handicaps.h"

/* ai/default */
#include "daidata.h"

#include "deepai.h"
#include "deepops.h"
#include "deeptuning.h"

#include "deepdiplomacy.h"

/* The cease-fire window, in turns left before the pact lapses back to
 * war. NOT a deep constant and not tunable: it is ai/default's own
 * refusal bar, `ds->turns_left > 4` at daidiplomacy.c:405, and deep
 * reads it so its proposal lands on a turn the counterparty's
 * valuation is allowed to say yes on. Changing it here would not
 * change the bar; it would only make deep propose into a refusal. */
#define DEEP_PEACE_WINDOW 4

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
  int love = 0, bar = 0;
  const char *why = "";
  bool wanted;

  if (ds->type != DS_CEASEFIRE
      || ds->turns_left > DEEP_PEACE_WINDOW
      || ds->turns_left <= 0) {
    /* Not a window. Counted nowhere: the census denominator is the
     * population the lever could act on, not every pair alive. */
    return;
  }

  dp->peace_windows++;
  wanted = deep_peace_wanted(ait, pplayer, aplayer, &love, &bar, &why);

  if (wanted) {
    dp->peace_proposed++;
    deep_treaty_clear(pplayer, aplayer);
    deep_treaty_suggest(pplayer, aplayer, CLAUSE_PEACE, FALSE, 0);
  }

  log_base(deep_metric_log_level(),
           "deep-peaceseek turn=%d plr=\"%s\" to=\"%s\" left=%d love=%d"
           " bar=%d sent=%d why=\"%s\"",
           game.info.turn, player_name(pplayer), player_name(aplayer),
           ds->turns_left, love, bar, wanted ? 1 : 0, why);
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
           " prodscore=%d wincap=%d bias=%d",
           game.info.turn, player_name(pplayer), dp->tech_leader_id,
           dp->prod_leader_id, tech_best, prod_best,
           dp->wincap_seen ? 1 : 0,
           (dp->tech_leader_id >= 0 || dp->prod_leader_id >= 0) ? 1 : 0);
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
  int n = 0, peace = 0;

  clause_list_iterate(ptreaty->clauses, pclause) {
    n++;
    if (pclause->type == CLAUSE_PEACE) {
      peace++;
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
  if (t->peace_seek_gain <= 0.0) {
    return;
  }

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || is_barbarian(aplayer)) {
      continue;
    }
    deep_peace_seek(ait, pplayer, aplayer);
  } players_iterate_alive_end;
}
