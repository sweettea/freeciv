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
#ifndef FC__DEEPREQUEST_H
#define FC__DEEPREQUEST_H

/* common */
#include "fc_types.h"

/* CHATTY slice 2: deep LISTENS. Slice 1 gave deep a voice and said in
 * its own header that it reads nothing anybody says back; this is the
 * module that reads, and every honesty property slice 1 claimed has to
 * be re-argued here rather than inherited. The four that hold:
 *
 * 1. THE SENDER IS THE SERVER'S. server/handchat.c hands us
 *    sender->playing, the player the connection is attached to. Deep
 *    never takes a name out of the text as an identity, so a line
 *    cannot claim to be from somebody it is not.
 * 2. A REQUEST IS NOT AN ORDER. Every command is priced through the
 *    seam that already prices that kind of decision -- the city's
 *    danger figure, the operation target's score, the trunk plan's
 *    choice of ally endpoint -- and every one of them can still lose
 *    to something deep wants more. Deep answers what it will do, and
 *    refuses in plain words when it will not.
 * 3. NOTHING IS LEARNED FROM THE TEXT. A city named in a request is
 *    resolved only against cities deep may already act on: its own,
 *    the asker's, or an enemy city on a tile deep has actually seen
 *    (map_is_known). A name deep cannot ground is refused, so an ally
 *    cannot use the channel to hand deep a map it has not scouted.
 * 4. EVERY EFFECT DECAYS. A request lives chatty_request_turns turns
 *    and then stops counting, and it stops counting the moment the
 *    alliance that carried it ends. There is no permanent state a
 *    talkative ally can accumulate.
 *
 * INERT WITHOUT MESSAGES. Nothing in this module runs on a turn
 * boundary except the expiry sweep, which is a loop over
 * DEEP_REQUESTS empty slots when nothing was ever said. With
 * chatty_inbound_gate at 0 even the parser returns on its first
 * comparison. */

struct city;
struct player;
struct tile;

/* What was asked for. The protocol is exact-match and there are four
 * commands; `status` is answered on the spot and stores nothing, so it
 * has no kind here. */
enum deep_request_kind {
  DEEP_REQ_DEFEND = 0,          /* defend <city>, one of OURS -- see the
                                 * refusal in deep_request_hear() for
                                 * why an ally's own city is not one */
  DEEP_REQ_ATTACK,              /* attack <city|player> */
  DEEP_REQ_ROAD,                /* road to <city> */
};

/* How many live requests one deep player remembers. A bookkeeping
 * bound like DEEP_CHAT_MEMOS and not a tunable: overflowing it can
 * only drop the OLDEST request early, which is the same thing the
 * decay does a few turns later. */
#define DEEP_REQUESTS 8

struct deep_request {
  short kind;                   /* enum deep_request_kind PLUS ONE, so a
                                 * zeroed (never written) slot cannot
                                 * read as DEEP_REQ_DEFEND. 0 = free. */
  int city_id;                  /* the named city, or 0 */
  int plr_no;                   /* the named player, or -1 */
  int from;                     /* player number of the ally who asked */
  int until;                    /* last turn on which this still counts */
};

/* One line of chat, addressed to this deep player by `sender`. Returns
 * without doing anything at all when the gate is shut, and silently
 * when the line is not one of the four commands -- ordinary
 * conversation must not be answered as if it were a protocol error. */
void deep_request_hear(struct player *pplayer, struct player *sender,
                       const char *text);

/* Start of this player's phase: retire what has expired and emit the
 * census line. */
void deep_request_phase_begin(struct player *pplayer);

/* The three prices. Each returns the neutral value (0, or 1.0) unless
 * a live request from a still-allied player names the thing. */
unsigned int deep_request_defend_fear(const struct player *pplayer,
                                      const struct city *pcity);
double deep_request_attack_mult(const struct player *pplayer,
                                const struct city *target);
bool deep_request_road_wanted(const struct player *pplayer,
                              const struct city *acity);

#endif /* FC__DEEPREQUEST_H */
