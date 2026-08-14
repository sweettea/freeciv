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
#include "log.h"
#include "shared.h"
#include "support.h"

/* common */
#include "ai.h"
#include "game.h"
#include "player.h"
#include "research.h"
#include "tech.h"
#include "unittype.h"

/* ai/default */
#include "daidata.h"
#include "daiplayer.h"

#include "deeptuning.h"

#include "deeptechcensus.h"

/* The site names as they appear in the log stream. Index-matched to
 * enum deep_tc_site; a mismatch is caught by the assert in
 * deep_techcensus_mark(). */
static const char *const site_name[DEEP_TC_SITE_COUNT] = {
  "other",
  "stock_assess",
  "stock_defender",
  "stock_attacker",
  "stock_diplomat",
  "stock_paradrop",
  "stock_domestic",
  "stock_building",
  "stock_gov",
  "stock_effects",
  "deep_techwall",
  "deep_horizon",
  "deep_settler"
};

/* One site's tally for one player for one turn. `total` is signed --
 * dont_want_tech_obsoleting_impr() writes negative wants and pretending
 * otherwise would inflate every economic bucket. */
struct deep_tc_bucket {
  double total;
  double mil;
  int writes;
};

struct deep_tc_state {
  bool armed;                             /* Snapshot has been taken */
  adv_want snap[A_LAST + 1];
  struct deep_tc_bucket bucket[DEEP_TC_SITE_COUNT];
};

static struct deep_tc_state tc_state[MAX_NUM_PLAYER_SLOTS];

/* Advance classification cache, derived from the ruleset's unit types
 * and therefore invalidated with them. 0 = unknown, 1 = military,
 * 2 = not military. */
static unsigned char mil_advance[A_LAST + 1];
static bool mil_advance_built = FALSE;

/**********************************************************************//**
  Build the advance classification: an advance is MILITARY-ENABLING when
  at least one non-civilian unit type names it as a present advance
  requirement in its build_reqs.

  Deliberately ruleset-derived rather than a hand-listed set of techs:
  this is the only definition that means the same thing in classic and
  in civ2civ3, and it is the same predicate the ai/deep techwall writer
  already walks (deepdefence.c:2136). It classifies the TARGET of a
  write, which is the one quantity computable identically for a deep
  seat (where deep can bracket the writers) and a stock seat (where it
  cannot).

  It over-counts dual-use advances -- Bronze Working enables Phalanx and
  Colossus alike -- and that is stated rather than corrected, because
  the ratio it feeds is compared BETWEEN SIDES on the same corpus, where
  a constant over-count divides out.
**************************************************************************/
static void build_mil_advance(void)
{
  memset(mil_advance, 0, sizeof(mil_advance));

  unit_type_iterate(putype) {
    if (utype_has_flag(putype, UTYF_CIVILIAN)) {
      continue;
    }
    requirement_vector_iterate(&putype->build_reqs, preq) {
      if (VUT_ADVANCE == preq->source.kind && preq->present) {
        int idx = advance_index(preq->source.value.advance);

        if (idx >= 0 && idx <= A_LAST) {
          mil_advance[idx] = 1;
        }
      }
    } requirement_vector_iterate_end;
  } unit_type_iterate_end;

  advance_index_iterate(A_FIRST, i) {
    if (mil_advance[i] == 0) {
      mil_advance[i] = 2;
    }
  } advance_index_iterate_end;

  mil_advance_built = TRUE;
}

/**********************************************************************//**
  Is this advance one that a non-civilian unit type is gated on?
**************************************************************************/
static bool advance_is_military(int idx)
{
  if (!mil_advance_built) {
    build_mil_advance();
  }

  return idx >= 0 && idx <= A_LAST && mil_advance[idx] == 1;
}

/**********************************************************************//**
  Drop the classification cache (ruleset reload).
**************************************************************************/
void deep_techcensus_ruleset_reset(void)
{
  mil_advance_built = FALSE;
}

/**********************************************************************//**
  The player's live want array, or NULL when there is nothing to read.
**************************************************************************/
static const struct ai_plr *want_array(const struct player *pplayer,
                                       struct ai_type *ait)
{
  if (pplayer == NULL || ait == NULL) {
    return NULL;
  }

  return def_ai_player_data(pplayer, ait);
}

/**********************************************************************//**
  Copy the live array into this player's snapshot.
**************************************************************************/
static void take_snapshot(struct deep_tc_state *st,
                          const struct ai_plr *plr_data)
{
  advance_index_iterate(A_FIRST, i) {
    st->snap[i] = plr_data->tech_want[i];
  } advance_index_iterate_end;
  st->armed = TRUE;
}

/**********************************************************************//**
  Charge everything written since the previous mark to `site`.

  See the header for why this is the attribution mechanism and not a log
  line at the writer: rule 2 puts every military writer out of reach.
**************************************************************************/
void deep_techcensus_mark(struct ai_type *ait, struct player *pplayer,
                          enum deep_tc_site site)
{
  const struct ai_plr *plr_data = want_array(pplayer, ait);
  struct deep_tc_state *st;
  struct deep_tc_bucket *b;

  fc_assert_ret(site >= 0 && site < DEEP_TC_SITE_COUNT);
  if (plr_data == NULL) {
    return;
  }
  st = &tc_state[player_index(pplayer)];
  if (!st->armed) {
    /* Nothing to diff against yet; this mark only establishes the
     * baseline. */
    take_snapshot(st, plr_data);
    return;
  }

  b = &st->bucket[site];
  advance_index_iterate(A_FIRST, i) {
    double delta = (double) plr_data->tech_want[i] - (double) st->snap[i];

    if (delta == 0.0) {
      continue;
    }
    b->total += delta;
    if (advance_is_military(i)) {
      b->mil += delta;
    }
    b->writes++;
    st->snap[i] = plr_data->tech_want[i];
  } advance_index_iterate_end;
}

/**********************************************************************//**
  Re-snapshot without charging anyone -- for the clear, which is not a
  writer.
**************************************************************************/
void deep_techcensus_resync(struct ai_type *ait, struct player *pplayer)
{
  const struct ai_plr *plr_data = want_array(pplayer, ait);

  if (plr_data == NULL) {
    return;
  }
  take_snapshot(&tc_state[player_index(pplayer)], plr_data);
}

/**********************************************************************//**
  The module this player's wants are actually written by, or NULL when
  it is not one whose `struct ai_plr` we may read.

  ai/deep and ai/classic share ai/default's `struct ai_plr` and its
  allocator, so either module's slot can be read through
  def_ai_player_data(). ai/baseline carries its own frozen COPY of that
  struct, so its slot is a different type and reading it here would be
  type punning -- it is refused, and a mixed_db corpus therefore
  reports deep seats only.
**************************************************************************/
static struct ai_type *readable_module(const struct player *pplayer)
{
  const struct ai_type *ait;

  if (!is_ai(pplayer)) {
    return NULL;
  }
  if (is_barbarian(pplayer)) {
    /* Barbarians and the animal kingdom are AI players seated on a
     * module, so they answer readable_module()'s other tests -- and
     * they are seated on CLASSIC whatever the arms are, which would
     * put a permanently-empty array and a nonsense research goal on
     * the stock side of every comparison. Measured before this guard:
     * a 4-player ALL-DEEP smoke game reported a stock seat -- "Lion",
     * the animal kingdom, total=0.0 every turn with a goal of
     * Advanced Flight. */
    return NULL;
  }
  ait = pplayer->ai;
  if (ait == NULL) {
    return NULL;
  }
  if (0 != strcmp(ait->name, "deep") && 0 != strcmp(ait->name, "classic")) {
    return NULL;
  }

  return (struct ai_type *) ait;
}

/**********************************************************************//**
  Is the advance this player is heading for one a non-civilian unit is
  gated on? -1 when there is no such advance to classify.
**************************************************************************/
static int goal_military(Tech_type_id tech)
{
  if (!valid_advance_by_number(tech)) {
    return -1;
  }

  return advance_is_military(tech) ? 1 : 0;
}

/**********************************************************************//**
  The name of an advance for the log, or "-" when there is none.
**************************************************************************/
static const char *goal_name(Tech_type_id tech)
{
  if (!valid_advance_by_number(tech)) {
    return "-";
  }

  return advance_rule_name(advance_by_number(tech));
}

/**********************************************************************//**
  Once per turn: the end-of-turn totals for every readable AI player,
  and the per-site breakdown for the deep ones.

  Called from a deep player's phase_finished, which end_phase() runs
  AFTER last_activities for every player (srv_main.c:1526 then :1598),
  so each player's array holds this turn's complete writes. That is the
  only point at which a stock seat's appetite is observable from here.
**************************************************************************/
void deep_techcensus_turn_report(void)
{
  static int reported_turn = -1;

  if (reported_turn == game.info.turn) {
    return;
  }
  reported_turn = game.info.turn;

  players_iterate_alive(pplayer) {
    struct ai_type *ait = readable_module(pplayer);
    const struct ai_plr *plr_data;
    const struct research *presearch;
    double total = 0.0, mil = 0.0, neg = 0.0;
    /* Want-weighted distance. dai_select_tech() does not choose the
     * advance with the largest want -- it spreads each want over the
     * advances on the path to it, DIVIDED BY THE STEP COUNT, and then
     * takes the argmax among advances that are gettable right now
     * (daitech.c:116-131). So a want of 1000 four steps away puts 250
     * on each prerequisite and loses to a want of 300 sitting on a
     * gettable tech. Appetite and direction are therefore different
     * quantities, and this is the one that decides direction. */
    double milsteps = 0.0, econsteps = 0.0;
    double milpos = 0.0, econpos = 0.0;
    double milnow = 0.0, econnow = 0.0;
    int advs = 0;
    bool deep_seat;

    if (ait == NULL) {
      continue;
    }
    plr_data = def_ai_player_data(pplayer, ait);
    if (plr_data == NULL) {
      continue;
    }
    deep_seat = (0 == strcmp(ait->name, "deep"));
    presearch = research_get(pplayer);

    advance_index_iterate(A_FIRST, i) {
      double w = (double) plr_data->tech_want[i];

      if (w == 0.0) {
        continue;
      }
      advs++;
      total += w;
      if (w < 0.0) {
        neg += w;
      }
      if (advance_is_military(i)) {
        mil += w;
      }
      if (w > 0.0 && presearch != NULL) {
        /* Distance stats over POSITIVE want only: a negative want does
         * not "sit" anywhere, it cancels one that does. */
        int steps = research_goal_unknown_techs(presearch, i);

        if (steps > 0) {
          if (advance_is_military(i)) {
            milpos += w;
            milsteps += w * steps;
            if (steps == 1) {
              milnow += w;
            }
          } else {
            econpos += w;
            econsteps += w * steps;
            if (steps == 1) {
              econnow += w;
            }
          }
        }
      }
    } advance_index_iterate_end;

    log_base(deep_metric_log_level(),
             "deep-techwant turn=%d player=\"%s\" side=\"%s\""
             " total=%.1f mil=%.1f econ=%.1f neg=%.1f advs=%d"
             " milsteps=%.2f econsteps=%.2f milnow=%.1f econnow=%.1f"
             " res=\"%s\" resmil=%d goal=\"%s\" goalmil=%d",
             game.info.turn, player_name(pplayer),
             deep_seat ? "deep" : "stock",
             total, mil, total - mil, neg, advs,
             milpos > 0.0 ? milsteps / milpos : 0.0,
             econpos > 0.0 ? econsteps / econpos : 0.0,
             milpos > 0.0 ? 100.0 * milnow / milpos : 0.0,
             econpos > 0.0 ? 100.0 * econnow / econpos : 0.0,
             presearch == NULL ? "-" : goal_name(presearch->researching),
             presearch == NULL ? -1 : goal_military(presearch->researching),
             presearch == NULL ? "-" : goal_name(presearch->tech_goal),
             presearch == NULL ? -1 : goal_military(presearch->tech_goal));

    if (deep_seat) {
      struct deep_tc_state *st = &tc_state[player_index(pplayer)];
      int s;

      for (s = 0; s < DEEP_TC_SITE_COUNT; s++) {
        if (st->bucket[s].writes == 0) {
          /* A site that wrote nothing this turn is reported as a zero
           * line anyway: "this writer did not fire" is the finding
           * this census exists to record, and a missing line cannot be
           * told from a missing mark. */
          log_base(deep_metric_log_level(),
                   "deep-techsite turn=%d player=\"%s\" site=\"%s\""
                   " writes=0 total=0.0 mil=0.0",
                   game.info.turn, player_name(pplayer), site_name[s]);
        } else {
          log_base(deep_metric_log_level(),
                   "deep-techsite turn=%d player=\"%s\" site=\"%s\""
                   " writes=%d total=%.1f mil=%.1f",
                   game.info.turn, player_name(pplayer), site_name[s],
                   st->bucket[s].writes, st->bucket[s].total,
                   st->bucket[s].mil);
        }
        st->bucket[s].total = 0.0;
        st->bucket[s].mil = 0.0;
        st->bucket[s].writes = 0;
      }
    }
  } players_iterate_alive_end;
}
