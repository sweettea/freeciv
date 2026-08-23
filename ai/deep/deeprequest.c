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

#include <string.h>

/* utility */
#include "fcintl.h"
#include "log.h"
#include "shared.h"
#include "support.h"

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "unit.h"
#include "unitlist.h"

/* server */
#include "maphand.h"

#include "deepai.h"
#include "deepchat.h"
#include "deepops.h"
#include "deeptuning.h"

#include "deeprequest.h"

/**********************************************************************//**
  THE INBOUND CHANNEL, cited.

  handle_chat_msg_req() (server/handchat.c:320) is the one funnel every
  client chat line passes through. Its four destinations all deliver by
  walking a conn_list, and an AI player holds no connection -- so a
  line addressed to deep reached nobody at all until this branch added
  the observation hook at the end of chat_msg_to_player(). That hook is
  struct ai_type::funcs.player_chat_msg, taken from the reserved slot
  the header keeps for exactly this, nullptr in every other module, and
  called AFTER every existing delivery, so no stock behaviour moved.

  Addressed chat ("Deep: defend Roma"), not the allies channel, because
  it is the only path that carries a server-authenticated sender
  together with an unambiguous addressee -- and the only one a stranger
  can reach at all, which is what makes the refusal reachable.
**************************************************************************/

/**********************************************************************//**
  Is this remembered request still worth anything? Three ways to stop
  counting, and the last one is the point: an ally who is no longer an
  ally has no standing to have asked.
**************************************************************************/
static bool deep_request_live(const struct player *pplayer,
                              const struct deep_request *req)
{
  const struct player *from;

  if (req->kind == 0 || game.info.turn > req->until) {
    return FALSE;
  }
  from = player_by_number(req->from);

  return from != NULL && from->is_alive && pplayers_allied(pplayer, from);
}

/**********************************************************************//**
  Remember one request, or refresh the one already standing for the
  same subject. Refreshing rather than appending is what stops an ally
  repeating "defend Roma" every turn from filling the ring and pushing
  out three other requests to say one thing three times.

  A full ring overwrites its OLDEST slot, which can only retire a
  request a few turns before the decay would have.
**************************************************************************/
static void deep_request_store(struct player *pplayer,
                               enum deep_request_kind kind, int city_id,
                               int plr_no, const struct player *from)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  struct deep_plr *dp = deep_plr_data(pplayer);
  int until = game.info.turn + tune->chatty_request_turns;
  int i;

  for (i = 0; i < DEEP_REQUESTS; i++) {
    struct deep_request *req = &dp->chat_req[i];

    if (req->kind == (short) kind + 1 && req->city_id == city_id
        && req->plr_no == plr_no && req->from == player_number(from)) {
      req->until = until;
      return;
    }
  }

  dp->chat_req[dp->chat_req_next].kind = (short) kind + 1;
  dp->chat_req[dp->chat_req_next].city_id = city_id;
  dp->chat_req[dp->chat_req_next].plr_no = plr_no;
  dp->chat_req[dp->chat_req_next].from = player_number(from);
  dp->chat_req[dp->chat_req_next].until = until;
  dp->chat_req_next = (dp->chat_req_next + 1) % DEEP_REQUESTS;
}

/**********************************************************************//**
  A city deep is ALLOWED to be asked to defend: one of its own, or one
  of the asker's. Not "any city with that name" -- deep must not learn
  from a sentence that a city exists somewhere it has never been.
**************************************************************************/
static struct city *deep_request_own_or_ally_city(struct player *pplayer,
                                                  struct player *sender,
                                                  const char *name)
{
  struct city *pcity = city_list_find_name(pplayer->cities, name);

  if (pcity == NULL) {
    pcity = city_list_find_name(sender->cities, name);
  }

  return pcity;
}

/**********************************************************************//**
  An enemy city deep could name for itself: owned by somebody deep is
  at war with, and standing on a tile deep has actually seen. The
  map_is_known() test is the same one deep_op_pick_target() already
  applies to its own candidates (deepops.c), which is why an accepted
  attack request can never widen the target set deep would have
  considered unaided -- it can only re-order it.
**************************************************************************/
static struct city *deep_request_enemy_city(struct player *pplayer,
                                            const char *name)
{
  players_iterate_alive(aplayer) {
    struct city *pcity;

    if (aplayer == pplayer || !pplayers_at_war(pplayer, aplayer)) {
      continue;
    }
    pcity = city_list_find_name(aplayer->cities, name);
    if (pcity != NULL && map_is_known(city_tile(pcity), pplayer)) {
      return pcity;
    }
  } players_iterate_alive_end;

  return NULL;
}

/**********************************************************************//**
  Have we got a city on the same landmass? The honest, cheap answer to
  "can you reach it" for a request about ground defence: continent
  numbers are maintained by assign_continent_numbers() and cost
  nothing, and a promise to garrison a city across an ocean is a
  promise deep's own defence pass will not keep.
**************************************************************************/
static bool deep_request_same_landmass(const struct player *pplayer,
                                       const struct city *pcity)
{
  Continent_id there = tile_continent(city_tile(pcity));

  city_list_iterate(pplayer->cities, own) {
    if (tile_continent(city_tile(own)) == there) {
      return TRUE;
    }
  } city_list_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  The `status` line: deep's stance, the operation it is actually
  running, and the worst thing it can see bearing down on the asker.

  Every field is something deep already computed for its own purposes
  and every one is filtered through deep's own sight
  (can_player_see_unit), so the answer cannot tell the ally anything a
  scout of ours standing where we stand could not.
**************************************************************************/
static void deep_request_status(struct player *pplayer,
                                struct player *sender)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  const char *opname = NULL;
  const char *threat = NULL;
  int nearest = FC_INFINITY;
  int i;

  for (i = 0; i < DEEP_OPS_MAX; i++) {
    if (dp->ops[i].state != DEEP_OP_NONE && dp->ops[i].target_city > 0) {
      struct city *target = game_city_by_number(dp->ops[i].target_city);

      if (target != NULL) {
        opname = city_name_get(target);
        break;
      }
    }
  }

  city_list_iterate(sender->cities, acity) {
    players_iterate_alive(eplayer) {
      if (eplayer == pplayer || eplayer == sender
          || !pplayers_at_war(sender, eplayer)
          || pplayers_allied(pplayer, eplayer)) {
        continue;
      }
      unit_list_iterate(eplayer->units, punit) {
        int eta;

        if (!is_attack_unit(punit) || !can_player_see_unit(pplayer, punit)) {
          continue;
        }
        eta = real_map_distance(unit_tile(punit), city_tile(acity))
              * SINGLE_MOVE / MAX(unit_type_get(punit)->move_rate, 1);
        if (eta <= tune->survival_horizon && eta < nearest) {
          nearest = eta;
          threat = player_name(eplayer);
        }
      } unit_list_iterate_end;
    } players_iterate_alive_end;
  } city_list_iterate_end;

  if (threat != NULL) {
    deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                    /* TRANS: an AI reporting its posture to an ally. */
                    _("Stance economy %.2f, force %.2f; marching on %s;"
                      " nearest threat to you is %s, eta %d."),
                    dp->stance.econ, dp->stance.mil,
                    opname != NULL ? opname : _("nobody"), threat, nearest);
  } else {
    deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                    /* TRANS: an AI reporting its posture to an ally. */
                    _("Stance economy %.2f, force %.2f; marching on %s;"
                      " we see nothing bearing down on you."),
                    dp->stance.econ, dp->stance.mil,
                    opname != NULL ? opname : _("nobody"));
  }
}

/**********************************************************************//**
  The verdict, so the smoke can read back what happened to a request
  without a client attached. One line per line HEARD, including the
  ones deep refused and the ones the cap swallowed.
**************************************************************************/
static void deep_request_log(const struct player *pplayer,
                             const struct player *sender, const char *text,
                             const char *verdict, int city_id, int until)
{
  log_base(deep_metric_log_level(),
           "deep-request turn=%d player=\"%s\" from=\"%s\" verdict=%s"
           " city=%d until=%d text=\"%s\"",
           game.info.turn, player_name(pplayer),
           sender != NULL ? player_name(sender) : "(none)",
           verdict, city_id, until, text);
}

/**********************************************************************//**
  One chat line addressed to this deep player.

  Order of the tests is the design, not an accident:

  * the gate first, so a shut gate costs one comparison;
  * the VERB before the sender, so that ordinary conversation from an
    ally -- or from anybody else -- is passed over in silence instead
    of being answered with a protocol complaint. Deep only ever speaks
    about a line that was addressed to it AND looks like one of its
    four commands;
  * the sender after the verb, so a stranger who does use the protocol
    gets the one honest answer there is;
  * the per-ally cap last, so that the request that exceeds it is
    counted and dropped rather than answered.
**************************************************************************/
void deep_request_hear(struct player *pplayer, struct player *sender,
                       const char *text)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  struct deep_plr *dp;
  char buf[MAX_LEN_MSG], *arg;
  enum deep_request_kind kind;
  struct city *pcity = NULL;
  struct player *tplayer = NULL;
  bool is_status = FALSE;
  int until;

  if (tune->chatty_inbound_gate <= 0 || text == NULL) {
    return;
  }
  dp = deep_plr_data(pplayer);
  if (dp == NULL) {
    return;
  }

  sz_strlcpy(buf, text);
  remove_leading_trailing_spaces(buf);

  /* The protocol, exact-match, no NLP: a verb we know, then the rest of
   * the line as one name. Case is folded on the VERB only -- a city is
   * matched by city_list_find_name(), which folds case itself. */
  if (0 == fc_strncasecmp(buf, "defend ", 7)) {
    kind = DEEP_REQ_DEFEND;
    arg = buf + 7;
  } else if (0 == fc_strncasecmp(buf, "attack ", 7)) {
    kind = DEEP_REQ_ATTACK;
    arg = buf + 7;
  } else if (0 == fc_strncasecmp(buf, "road to ", 8)) {
    kind = DEEP_REQ_ROAD;
    arg = buf + 8;
  } else if (0 == fc_strcasecmp(buf, "status")) {
    kind = DEEP_REQ_DEFEND;             /* unread; see is_status */
    is_status = TRUE;
    arg = buf;
  } else {
    /* Not a command. Deep was talked to, not instructed. */
    return;
  }
  remove_leading_trailing_spaces(arg);

  /* Only an ally may ask, and a stranger who tried is told so once --
   * through the same rate-capped channel, so the refusal cannot be
   * used to make deep flood its own allies' chat windows. */
  if (sender == NULL || sender == pplayer
      || !pplayers_allied(pplayer, sender)) {
    dp->req_refused_stranger++;
    deep_request_log(pplayer, sender, buf, "not-ally", 0, 0);
    if (sender != NULL
        && deep_chat_once(pplayer, DEEP_CHAT_REPLY,
                          player_number(sender), -1)) {
      /* Not deep_chat_tell(): that one talks to allies only, which is
       * exactly who this is not. The line goes back to where it came
       * from, once per stranger, out of the same per-turn cap. */
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                      _("We take requests from our allies only."));
    }
    return;
  }

  /* The per-ally cap, reusing slice 1's number rather than adding a
   * second one: chatty_max_per_turn already answers "how much traffic
   * is one turn of this channel worth", and a request deep answers
   * costs an outbound line anyway. */
  if (dp->chat_req_turn != game.info.turn) {
    dp->chat_req_turn = game.info.turn;
    memset(dp->chat_req_heard, 0, sizeof(dp->chat_req_heard));
  }
  if (dp->chat_req_heard[player_index(sender)] >= tune->chatty_max_per_turn) {
    dp->req_capped++;
    deep_request_log(pplayer, sender, buf, "rate-capped", 0, 0);
    return;
  }
  dp->chat_req_heard[player_index(sender)]++;
  dp->req_heard++;

  if (is_status) {
    deep_request_log(pplayer, sender, buf, "status", 0, 0);
    deep_request_status(pplayer, sender);
    return;
  }

  until = game.info.turn + tune->chatty_request_turns;

  switch (kind) {
  case DEEP_REQ_DEFEND:
    pcity = deep_request_own_or_ally_city(pplayer, sender, arg);
    if (pcity == NULL) {
      deep_request_log(pplayer, sender, buf, "no-such-city", 0, 0);
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                     _("We hold no city called %s, and neither do you."),
                     arg);
      return;
    }
    if (!deep_request_same_landmass(pplayer, pcity)) {
      deep_request_log(pplayer, sender, buf, "unreachable", pcity->id, 0);
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                     _("We cannot reach %s by land."),
                     city_name_get(pcity));
      return;
    }
    deep_request_store(pplayer, kind, pcity->id, -1, sender);
    dp->req_taken++;
    deep_request_log(pplayer, sender, buf, "defend", pcity->id, until);
    deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                   _("We will reinforce %s."), city_name_get(pcity));
    return;

  case DEEP_REQ_ATTACK:
    tplayer = player_by_name(arg);
    if (tplayer != NULL) {
      if (tplayer == pplayer || !pplayers_at_war(pplayer, tplayer)) {
        deep_request_log(pplayer, sender, buf, "not-at-war", 0, 0);
        deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                       _("We are not at war with %s."),
                       player_name(tplayer));
        return;
      }
      deep_request_store(pplayer, kind, 0, player_number(tplayer), sender);
      dp->req_taken++;
      deep_request_log(pplayer, sender, buf, "attack-player", 0, until);
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                     _("We will press the war against %s."),
                     player_name(tplayer));
      return;
    }
    pcity = deep_request_enemy_city(pplayer, arg);
    if (pcity == NULL) {
      deep_request_log(pplayer, sender, buf, "no-such-target", 0, 0);
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                     _("We have not seen a city called %s, nor met anybody"
                       " of that name."), arg);
      return;
    }
    deep_request_store(pplayer, kind, pcity->id, -1, sender);
    dp->req_taken++;
    deep_request_log(pplayer, sender, buf, "attack-city", pcity->id, until);
    deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                   _("We will weigh %s heavier as a target."),
                   city_name_get(pcity));
    return;

  case DEEP_REQ_ROAD:
    pcity = city_list_find_name(sender->cities, arg);
    if (pcity == NULL) {
      deep_request_log(pplayer, sender, buf, "not-your-city", 0, 0);
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                     _("We build corridors to our allies' own cities;"
                       " %s is not one of yours."), arg);
      return;
    }
    if (!deep_request_same_landmass(pplayer, pcity)) {
      deep_request_log(pplayer, sender, buf, "no-land-route", pcity->id, 0);
      deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                     _("There is no land route from our cities to %s."),
                     city_name_get(pcity));
      return;
    }
    deep_request_store(pplayer, kind, pcity->id, -1, sender);
    dp->req_taken++;
    deep_request_log(pplayer, sender, buf, "road", pcity->id, until);
    deep_chat_reply(pplayer, sender, DEEP_CHAT_REPLY,
                   _("We will run our corridor to %s."),
                   city_name_get(pcity));
    return;
  }
}

/**********************************************************************//**
  Retire what has expired, and say how many are standing. Runs at
  phase_begin so that the count a turn's decisions read is the count
  this line reports.
**************************************************************************/
void deep_request_phase_begin(struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int live = 0, retired = 0, i;

  if (dp == NULL) {
    return;
  }
  for (i = 0; i < DEEP_REQUESTS; i++) {
    if (dp->chat_req[i].kind == 0) {
      continue;
    }
    if (deep_request_live(pplayer, &dp->chat_req[i])) {
      live++;
    } else {
      dp->chat_req[i].kind = 0;
      retired++;
    }
  }

  if (live > 0 || retired > 0 || dp->req_heard > 0) {
    log_base(deep_metric_log_level(),
             "deep-requests turn=%d player=\"%s\" live=%d retired=%d"
             " heard=%d taken=%d capped=%d stranger=%d",
             game.info.turn, player_name(pplayer), live, retired,
             dp->req_heard, dp->req_taken, dp->req_capped,
             dp->req_refused_stranger);
  }
}

/**********************************************************************//**
  PRICE 1 -- defence. An extra fear term for a city an ally asked us to
  hold, in the same currency as every other injected fear
  (deepdefence.c): a magnitude added to city_data->danger, which the
  build path then squares and converts like any other danger. It is a
  term, not an override, so a city with nothing to fear and a request
  standing gets exactly chatty_defend_fear of want and still loses to a
  city genuinely under siege.
**************************************************************************/
unsigned int deep_request_defend_fear(const struct player *pplayer,
                                      const struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int i;

  if (tune->chatty_defend_fear <= 0.0 || dp == NULL) {
    return 0;
  }
  for (i = 0; i < DEEP_REQUESTS; i++) {
    const struct deep_request *req = &dp->chat_req[i];

    if (req->kind == (short) DEEP_REQ_DEFEND + 1
        && req->city_id == pcity->id
        && deep_request_live(pplayer, req)) {
      return (unsigned int) tune->chatty_defend_fear;
    }
  }

  return 0;
}

/**********************************************************************//**
  PRICE 2 -- attack. A multiplier on the operation target score, which
  is a pure RANKING quantity (worth per tile; nothing compares it to a
  threshold), so this can re-order deep's targets and can never let it
  attack something its own odds gate refused.
**************************************************************************/
double deep_request_attack_mult(const struct player *pplayer,
                                const struct city *target)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int i;

  if (tune->chatty_attack_gain <= 0.0 || dp == NULL) {
    return 1.0;
  }
  for (i = 0; i < DEEP_REQUESTS; i++) {
    const struct deep_request *req = &dp->chat_req[i];

    if (req->kind != (short) DEEP_REQ_ATTACK + 1
        || !deep_request_live(pplayer, req)) {
      continue;
    }
    if (req->city_id == target->id
        || (req->plr_no >= 0
            && req->plr_no == player_number(city_owner(target)))) {
      return 1.0 + tune->chatty_attack_gain;
    }
  }

  return 1.0;
}

/**********************************************************************//**
  PRICE 3 -- roads. TRUE when a live request names this ally city, so
  the trunk planner can prefer it as the alliance's endpoint. A
  preference over which of an ally's cities the corridor runs to, not a
  new corridor: with trunk_ally_gate shut there is no ally node to
  prefer and this changes nothing.
**************************************************************************/
bool deep_request_road_wanted(const struct player *pplayer,
                              const struct city *acity)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int i;

  if (tune->chatty_road_gain <= 0.0 || dp == NULL) {
    return FALSE;
  }
  for (i = 0; i < DEEP_REQUESTS; i++) {
    const struct deep_request *req = &dp->chat_req[i];

    if (req->kind == (short) DEEP_REQ_ROAD + 1
        && req->city_id == acity->id
        && deep_request_live(pplayer, req)) {
      return TRUE;
    }
  }

  return FALSE;
}
