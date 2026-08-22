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
#ifndef FC__DEEPCHAT_H
#define FC__DEEPCHAT_H

/* utility */
#include "support.h"            /* fc__attribute */

/* common */
#include "fc_types.h"

/* CHATTY: deep talks TO its allies about its own plans. OUTBOUND ONLY
 * -- nothing here ever reads a message, so there is no protocol, no
 * negotiation and no incoming text to be lied to by. Every line's
 * content is deep's OWN state (a corridor it planned, a threat its own
 * threat model saw, an operation it just launched, a pact it just
 * signed), which makes the channel honest by construction: it can only
 * leak what deep already knows.
 *
 * The transport is the one the stock AI already uses for player-visible
 * diplomatic text -- vpackage_event() + lsend_packet_chat_msg() +
 * event_cache_add_for_player(), which is exactly dai_diplo_notify()
 * (ai/default/daidiplomacy.c:103). No new transport, and the line
 * arrives in a connected client's chat window addressed from the deep
 * player in stock's own "*Name (AI)*" shape.
 *
 * OUTPUT ONLY. Nothing in this module writes game state, moves a unit,
 * touches a want or draws from the RNG, which is why a gate-on run and
 * a gate-off run of the same seed play the same game. */

struct ai_type;
struct city;
struct player;

/* What a line is about. Used for the memo key (so "once per city" and
 * "once per pact" cannot collide) and for the census tag. */
enum deep_chat_class {
  DEEP_CHAT_CORRIDOR = 0,       /* an ally corridor planned or finished */
  DEEP_CHAT_THREAT,             /* an enemy force closing on an ally city */
  DEEP_CHAT_OPERATION,          /* an assault launched at a named city */
  DEEP_CHAT_PACT,               /* peace, alliance or war declared */
};

/* One thing already said. A bookkeeping bound like DEEP_OP_MEMBERS and
 * not a tunable: it bounds how many DISTINCT subjects a player can
 * remember having spoken about, and overflowing it can only permit a
 * repeat, never a new class of line. */
#define DEEP_CHAT_MEMOS 64

struct deep_chat_memo {
  short klass;                  /* enum deep_chat_class, -1 = free */
  int key1;
  int key2;
};

/* TRUE while this player may still say something this turn: the gate
 * is open, the per-turn cap is not spent, and somebody is allied to
 * hear it. Callers test this BEFORE doing any work of their own, so
 * that a shut gate costs exactly one comparison and the scans below
 * never run. */
bool deep_chat_open(const struct player *pplayer);

/* Say one line to every ally, or to one named player. Both return TRUE
 * when the line was actually sent, and both spend one unit of the
 * per-turn cap per LINE (not per recipient). */
bool deep_chat_say(const struct player *pplayer, enum deep_chat_class klass,
                   const char *text, ...)
                   fc__attribute((__format__ (__printf__, 3, 4)));
bool deep_chat_tell(const struct player *pplayer,
                    const struct player *aplayer,
                    enum deep_chat_class klass, const char *text, ...)
                    fc__attribute((__format__ (__printf__, 4, 5)));

/* Dedupe: TRUE the first time this player raises this subject, FALSE
 * ever after. The subject is (class, key1, key2) -- a city id and a
 * player number, say. Nothing is remembered while the gate is shut. */
bool deep_chat_once(const struct player *pplayer, enum deep_chat_class klass,
                    int key1, int key2);

/* Start of this player's phase: the per-turn cap is fresh. */
void deep_chat_phase_begin(const struct player *pplayer);

#endif /* FC__DEEPCHAT_H */
