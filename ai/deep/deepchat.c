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

#include <stdarg.h>

/* utility */
#include "fcintl.h"
#include "log.h"
#include "support.h"

/* common */
#include "city.h"
#include "featured_text.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "player.h"
#include "unit.h"
#include "unitlist.h"

/* server */
#include "notify.h"

#include "deepai.h"
#include "deeptuning.h"

#include "deepchat.h"

/**********************************************************************//**
  The channel.

  THE TRANSPORT IS STOCK'S. dai_diplo_notify() (ai/default/daidiplomacy.c
  :103) is how the stock AI has always put player-visible text in a
  human's chat window: package the text as a chat-message packet with an
  event type and a featured-text colour, send it to the recipient's
  connections, and add it to the server's event cache so it survives a
  reconnect. This module does the same three calls in the same order
  against the same server API -- package_event() / lsend_packet_chat_msg()
  / event_cache_add_for_player(), all declared in server/notify.h and
  common/packets.h. Nothing here is a new transport, and deep does not
  copy stock's static wrapper: the wrapper is four lines of the public
  API, it answers to stock's own `diplomacy_verbose` rather than to
  deep's gate, and it addresses one fixed player.

  Two differences from the stock wrapper, both deliberate:

  * the colour is ftc_chat_ally rather than ftc_chat_private, because
    the recipient of every line here IS an ally and that is the colour
    the client already paints ally chat in;
  * the gate is deep's own chatty_gate plus a per-turn cap, not
    stock's diplomacy_verbose.

  The line's SHAPE is stock's, "*Name (AI)* text", so a human sees deep
  in the same place and the same form as any other AI that talks.

  OUTPUT ONLY. Everything in this file writes a packet, a log line and
  its own bookkeeping. It never touches a unit, a want, a tile or the
  RNG, which is what lets a gate-on game and a gate-off game of the same
  seed play out identically.
**************************************************************************/

/**********************************************************************//**
  Is the gate open, the per-turn cap unspent, and somebody allied there
  to hear it? The FIRST test every caller makes, so that a shut gate
  costs one comparison and none of the scans behind it ever run.
**************************************************************************/
bool deep_chat_open(const struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  struct deep_plr *dp;

  if (tune->chatty_gate <= 0 || tune->chatty_max_per_turn <= 0) {
    return FALSE;
  }
  dp = deep_plr_data(pplayer);
  if (dp == NULL) {
    return FALSE;
  }
  if (dp->chat_turn == game.info.turn
      && dp->chat_said >= tune->chatty_max_per_turn) {
    return FALSE;
  }

  players_iterate_alive(aplayer) {
    if (aplayer != pplayer && pplayers_allied(pplayer, aplayer)) {
      return TRUE;
    }
  } players_iterate_alive_end;

  return FALSE;
}

/**********************************************************************//**
  Send one already-formatted line to every ally, or to `only` when it
  is not NULL and is in fact an ally. Spends one unit of the per-turn
  cap per LINE rather than per recipient: the cap exists to bound how
  much deep says, and saying one thing to three allies is one thing.
**************************************************************************/
static bool deep_chat_send(const struct player *pplayer,
                           const struct player *only,
                           enum deep_chat_class klass, const char *body)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  struct deep_plr *dp = deep_plr_data(pplayer);
  struct packet_chat_msg packet;
  char line[MAX_LEN_MSG];
  int sent = 0;

  if (!deep_chat_open(pplayer)) {
    return FALSE;
  }

  /* Stock's shape, through the translation macros stock's own messages
   * go through: the whole visible line is one format string. */
  fc_snprintf(line, sizeof(line), _("*%s (AI)* %s"),
              player_name(pplayer), body);

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer
        || !pplayers_allied(pplayer, aplayer)
        || (only != NULL && aplayer != only)) {
      continue;
    }
    package_event(&packet, NULL, E_DIPLOMACY, ftc_chat_ally, "%s", line);
    lsend_packet_chat_msg(aplayer->connections, &packet);
    /* So the line survives a reconnect, exactly as stock's does. */
    event_cache_add_for_player(&packet, aplayer);
    sent++;
  } players_iterate_alive_end;

  if (sent == 0) {
    return FALSE;
  }

  if (dp->chat_turn != game.info.turn) {
    dp->chat_turn = game.info.turn;
    dp->chat_said = 0;
  }
  dp->chat_said++;

  /* The liveness instrument. A benchmark game has no client connected,
   * so the packet stream is invisible to it; this line is how the smoke
   * test reads back what was said, to whom, and how much of the cap it
   * spent. */
  log_base(deep_metric_log_level(),
           "deep-chat turn=%d player=\"%s\" class=%d to=\"%s\" heard=%d"
           " said=%d/%d text=\"%s\"",
           game.info.turn, player_name(pplayer), (int) klass,
           only != NULL ? player_name(only) : "allies", sent,
           dp->chat_said, tune->chatty_max_per_turn, line);

  return TRUE;
}

/**********************************************************************//**
  Say one line to every ally.
**************************************************************************/
bool deep_chat_say(const struct player *pplayer, enum deep_chat_class klass,
                   const char *text, ...)
{
  char body[MAX_LEN_MSG];
  va_list args;

  if (!deep_chat_open(pplayer)) {
    return FALSE;
  }
  va_start(args, text);
  fc_vsnprintf(body, sizeof(body), text, args);
  va_end(args);

  return deep_chat_send(pplayer, NULL, klass, body);
}

/**********************************************************************//**
  Say one line to one named ally. A non-ally is never told anything:
  this channel talks to the alliance and to nobody else.
**************************************************************************/
bool deep_chat_tell(const struct player *pplayer,
                    const struct player *aplayer,
                    enum deep_chat_class klass, const char *text, ...)
{
  char body[MAX_LEN_MSG];
  va_list args;

  if (aplayer == NULL || !deep_chat_open(pplayer)
      || !pplayers_allied(pplayer, aplayer)) {
    return FALSE;
  }
  va_start(args, text);
  fc_vsnprintf(body, sizeof(body), text, args);
  va_end(args);

  return deep_chat_send(pplayer, aplayer, klass, body);
}

/**********************************************************************//**
  TRUE the first time this player raises this subject, FALSE ever
  after. The ring is bounded (DEEP_CHAT_MEMOS) and overwrites its
  oldest entry, so the failure mode of a very long game is one repeated
  line and never a silence.

  Nothing is remembered while the gate is shut, because nothing is
  said: the memo is written by the caller only after deep_chat_open()
  has already answered TRUE.
**************************************************************************/
bool deep_chat_once(const struct player *pplayer, enum deep_chat_class klass,
                    int key1, int key2)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int i;

  if (dp == NULL) {
    return FALSE;
  }
  for (i = 0; i < DEEP_CHAT_MEMOS; i++) {
    if (dp->chat_memo[i].klass == (short) klass + 1
        && dp->chat_memo[i].key1 == key1
        && dp->chat_memo[i].key2 == key2) {
      return FALSE;
    }
  }
  /* klass + 1, so that a zeroed (never written) slot cannot read as
   * class 0 with keys 0,0 -- which is a real subject. */
  dp->chat_memo[dp->chat_memo_next].klass = (short) klass + 1;
  dp->chat_memo[dp->chat_memo_next].key1 = key1;
  dp->chat_memo[dp->chat_memo_next].key2 = key2;
  dp->chat_memo_next = (dp->chat_memo_next + 1) % DEEP_CHAT_MEMOS;

  return TRUE;
}

/**********************************************************************//**
  Start of this player's phase: the per-turn cap is fresh. Reset here
  rather than lazily on first use so that the tally a mid-turn line
  logs is this turn's and not the previous one's.
**************************************************************************/
void deep_chat_phase_begin(const struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);

  if (dp != NULL) {
    dp->chat_turn = game.info.turn;
    dp->chat_said = 0;
  }
}

/**********************************************************************//**
  Message class 2: an enemy force closing on an ALLY's city.

  Deep already asks this question about its own cities every turn --
  which at-war units can reach this city inside survival_horizon turns
  (deepdefence.c, deepcombat.c) -- and the answer for an ally's city is
  the same question asked of the same evidence. The evidence is DEEP'S
  OWN SIGHT: can_player_see_unit() is the filter, so the warning can
  only ever repeat something deep can see from where deep stands. It
  leaks nothing the ally could not have been told by a scout of ours
  standing in the same place, which is the honesty property of the
  whole channel.

  Reusing deep_city_defence_read() instead was rejected on purpose: it
  is deep's own defensive pricing, it memoises per city into the
  survival cache, and running it over an ally's cities would put
  entries in a cache deep's own decisions read this phase. This scan
  allocates nothing, caches nothing, and answers a strictly smaller
  question -- is anything hostile within reach, and how close.

  The window is survival_horizon and not a knob of its own: it is
  already deep's answer to "how far ahead is a threat a threat", and a
  second constant about the same question would be a second law.

  Once per (city, threatening player): the ally is told a thing is
  coming, not told again every turn while it comes.
**************************************************************************/
static void deep_chat_ally_threats(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_for(pplayer);

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || !pplayers_allied(pplayer, aplayer)) {
      continue;
    }

    city_list_iterate(aplayer->cities, acity) {
      if (!deep_chat_open(pplayer)) {
        return;
      }

      players_iterate_alive(eplayer) {
        int seen = 0, nearest = FC_INFINITY;

        if (eplayer == pplayer || eplayer == aplayer
            || !pplayers_at_war(aplayer, eplayer)
            || pplayers_allied(pplayer, eplayer)) {
          continue;
        }

        unit_list_iterate(eplayer->units, punit) {
          int eta;

          if (!is_attack_unit(punit)
              || !can_player_see_unit(pplayer, punit)) {
            continue;
          }
          eta = real_map_distance(unit_tile(punit), city_tile(acity))
                * SINGLE_MOVE / MAX(unit_type_get(punit)->move_rate, 1);
          if (eta <= tune->survival_horizon) {
            seen++;
            nearest = MIN(nearest, eta);
          }
        } unit_list_iterate_end;

        if (seen > 0
            && deep_chat_once(pplayer, DEEP_CHAT_THREAT, acity->id,
                              player_number(eplayer))) {
          deep_chat_tell(pplayer, aplayer, DEEP_CHAT_THREAT,
                         PL_("Enemy force approaching your city %s:"
                             " %d unit of %s, %d turns off.",
                             "Enemy force approaching your city %s:"
                             " %d units of %s, %d turns off.", seen),
                         city_name_get(acity), seen, player_name(eplayer),
                         nearest);
          if (!deep_chat_open(pplayer)) {
            return;
          }
        }
      } players_iterate_alive_end;
    } city_list_iterate_end;
  } players_iterate_alive_end;
}

/**********************************************************************//**
  End of this player's phase: the classes deep reads off its own state.
  Placed here rather than at phase_begin because this is after the
  turn's movement and after diplomacy_actions (srv_main.c:1390), so
  what deep reports is what actually happened this turn.
**************************************************************************/
void deep_chat_phase_finished(struct player *pplayer)
{
  if (!deep_chat_open(pplayer)) {
    return;
  }

  deep_chat_ally_threats(pplayer);
}
