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
#ifndef FC__DEEPTECHCENSUS_H
#define FC__DEEPTECHCENSUS_H

/* Tech-want writer census (TECHMIL slice 1). READ-ONLY: every function
 * here observes `struct ai_plr::tech_want[]` and emits a log line.
 * Nothing in this file writes a want, reads the RNG, or is reachable
 * from a decision, so a build with it in is byte-identical to one
 * without.
 *
 * WHY IT EXISTS. Deep registers no tech hook, so its research goal is
 * chosen by stock's `dai_select_tech()` from `tech_want[]`. The
 * MILITARY writers of that array live inside machinery deep replaced
 * -- `dai_process_defender_want()` (daimilitary.c:1195),
 * `process_attacker_want()` (daimilitary.c:1460, reached only through
 * `kill_something_with()`), `dai_wants_defender_against()`
 * (daitech.c:507, reached only through `assess_danger()`) and
 * `dai_wants_role_unit()` (daitech.c:602). If deep's build path does
 * not reach them, deep players have less military tech appetite than
 * stock players BY ACCIDENT rather than by decision. This module is
 * what says which.
 *
 * HOW IT ATTRIBUTES. Rule 2 forbids a log line inside `ai/default`, so
 * no writer can announce itself. Instead deep keeps a per-player
 * snapshot of the whole array and diffs it at the phase boundaries it
 * already owns: `deep_techcensus_mark()` charges everything that has
 * been written since the previous mark to the named site. Marks are
 * placed in PAIRS around each stock call deep makes -- an OTHER mark
 * to flush what came before, the call, then the site mark -- so the
 * attribution is exact for every site that is bracketed and
 * conservative (it lands in OTHER) for every one that is not. Nesting
 * needs no stack: an inner pair flushes before the outer pair closes.
 *
 * WHAT IT CANNOT SEE. Stock seats run `dai_do_last_activities()` in
 * `ai/default`, where deep has no place to stand. For a stock player
 * only the END-OF-TURN TOTAL is observable -- read from that player's
 * own AI slot in `deep_techcensus_turn_report()`, which is legal
 * because `dai_player_alloc()` runs for every player under every
 * registered module. Per-site attribution for stock seats is
 * UNATTRIBUTABLE by construction and is reported as such. The
 * cross-side comparison therefore uses a metric computable identically
 * on both sides: the class of the ADVANCE the want was written to. */

#include "fc_types.h"

/* The writer sites deep can bracket. Each names the ai/default (or
 * ai/deep) line that does the writing; the comment is the call deep
 * makes to reach it. */
enum deep_tc_site {
  /* Everything not bracketed by a pair below. */
  DEEP_TC_OTHER = 0,
  /* daitech.c:507, via assess_danger() (daimilitary.c:901). */
  DEEP_TC_STOCK_ASSESS,
  /* daimilitary.c:1203, via dai_process_defender_want(). */
  DEEP_TC_STOCK_DEFENDER,
  /* daimilitary.c:1460, via kill_something_with(). */
  DEEP_TC_STOCK_ATTACKER,
  /* daidiplomat.c:150, via dai_choose_diplomat_defensive(). */
  DEEP_TC_STOCK_DIPLOMAT,
  /* daiparadrop.c:419/431, via dai_choose_paratrooper(). */
  DEEP_TC_STOCK_PARADROP,
  /* daitech.c:602, via domestic_advisor_choose_build(). */
  DEEP_TC_STOCK_DOMESTIC,
  /* daicity.c:142, via building_advisor()'s build_adv_* hooks. */
  DEEP_TC_STOCK_BUILDING,
  /* daihand.c:736, via dai_manage_government(). */
  DEEP_TC_STOCK_GOV,
  /* daitech.c:325, via dai_manage_tech(). */
  DEEP_TC_STOCK_EFFECTS,
  /* deepdefence.c:2141, deep_defensive_techwall(). */
  DEEP_TC_DEEP_TECHWALL,
  /* deephorizon.c:1895, deep_adjust_building_wants(). */
  DEEP_TC_DEEP_HORIZON,
  /* deepsettler.c:1014, deep_find_best_city_placement(). */
  DEEP_TC_DEEP_SETTLER,
  DEEP_TC_SITE_COUNT
};

/* Charge every tech_want[] change since this player's previous mark to
 * `site`, then re-snapshot. Cheap and idempotent: a mark with nothing
 * written since the last one records nothing. */
void deep_techcensus_mark(struct ai_type *ait, struct player *pplayer,
                          enum deep_tc_site site);

/* Re-snapshot without charging anyone. The one legitimate use is
 * immediately after dai_clear_tech_wants(), whose wipe is not a
 * writer's doing and must not be charged to the next site. */
void deep_techcensus_resync(struct ai_type *ait, struct player *pplayer);

/* Once per turn, from a deep player's phase_finished (by which point
 * end_phase() has run last_activities for EVERY player): emit the
 * per-player end-of-turn totals for both sides and, for deep players
 * only, the per-site breakdown. Resets the per-turn site tallies. */
void deep_techcensus_turn_report(void);

/* Drop the advance-classification cache. Called when the ruleset's
 * unit types are (re)loaded, since the cache is derived from them. */
void deep_techcensus_ruleset_reset(void);

/* The census's advance classification, exported for the deep-techgoal
 * counter (deeptech.c) so both instruments mean the same thing by
 * "military": 1 = gates a non-civilian unit type, 0 = does not,
 * -1 = not a classifiable advance (A_UNSET, A_FUTURE, invalid). */
int deep_techcensus_advance_military(Tech_type_id tech);

#endif /* FC__DEEPTECHCENSUS_H */
