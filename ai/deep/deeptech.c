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

#include <math.h>

/* utility */
#include "log.h"
#include "shared.h"

/* common */
#include "ai.h"
#include "game.h"
#include "player.h"
#include "research.h"
#include "tech.h"

/* server */
#include "techtools.h"

/* ai/default */
#include "daidata.h"
#include "daiplayer.h"
#include "daitech.h"

/* ai/deep */
#include "deeptechcensus.h"
#include "deeptuning.h"

#include "deeptech.h"

/**********************************************************************//**
  An advance's name for the counter line, or "-" when there is none.
**************************************************************************/
static const char *tg_name(Tech_type_id tech)
{
  if (!valid_advance_by_number(tech)) {
    return "-";
  }

  return advance_rule_name(advance_by_number(tech));
}

/**********************************************************************//**
  Re-select the researching advance and the research goal from
  tech_want[], with the distance shape under deep's control.

  This is stock's `dai_select_tech()` selection (daitech.c:70) COPIED
  rather than called, stated per rule 3 with the reason: the function
  is static, and the visibility exception would only export a
  selection whose distance exponent is frozen at 1 -- the exponent
  being ours to move is the entire point of the slice. The copy
  deviates from the original in exactly three declared ways:

  1. Both distance divisions carry `power`: each want spreads over its
     goal's prerequisites at want/steps^power (stock: want/steps), and
     the goal argmax divides the accumulated path mass by steps^power
     (stock: /steps).
  2. Arithmetic is double throughout. Stock accumulates into int
     arrays, truncating every deposit; at power = 1.0 this copy is
     therefore stock's shape but not always stock's argmax, which is
     what the gate-on-at-1.0 A/B arm measures.
  3. It runs as a POST-pass, after `dai_manage_tech()` has committed
     stock's own selection. Stock's picks are read back here as the
     counter's honesty fields, and the switching-penalty guard is
     recomputed from the post-stock research state -- `choose_tech()`
     saves the abandoned line and never charges the techpenalty twice
     in one turn (techtools.c:1050), so a second same-turn switch is
     safe.

  Mirrored guards, in stock's order: humans in the research pool chose
  already (dai_manage_tech returned before selecting; so do we);
  researching a future tech with no real advance available keeps it.
**************************************************************************/
static void deep_reselect_research(struct ai_type *ait,
                                   struct player *pplayer,
                                   double power)
{
  struct research *presearch = research_get(pplayer);
  struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);
  int num_cities_nonzero = MAX(1, city_list_size(pplayer->cities));
  Tech_type_id ac = advance_count();
  double values[MAX(A_ARRAY_SIZE, A_UNSET + 1)];
  double goal_values[MAX(A_ARRAY_SIZE, A_UNSET + 1)];
  Tech_type_id newtech, newgoal, stock_res, stock_goal;
  int penalty, i_;
  bool switched = FALSE;

  if (presearch == NULL || plr_data == NULL) {
    return;
  }

  /* If there are humans in our team, they will choose the techs --
   * dai_manage_tech() selected nothing, and neither may we. */
  research_players_iterate(presearch, aplayer) {
    if (!is_ai(aplayer)) {
      return;
    }
  } research_players_iterate_end;

  if (is_future_tech(presearch->researching)) {
    bool real_found = FALSE;

    advance_index_iterate_max(A_FIRST, i, ac) {
      if (research_invention_state(presearch, i) == TECH_PREREQS_KNOWN) {
        real_found = TRUE;
        break;
      }
    } advance_index_iterate_max_end;

    if (!real_found) {
      /* Stock kept the future tech and unset the goal; keep both. */
      return;
    }
  }

  /* Stock's actual committed picks, made moments ago -- the honesty
   * fields of the counter below. */
  stock_res = presearch->researching;
  stock_goal = presearch->tech_goal;

  for (i_ = 0; i_ < (int) (sizeof(values) / sizeof(values[0])); i_++) {
    values[i_] = 0.0;
    goal_values[i_] = 0.0;
  }
  values[A_UNSET] = -1.0;
  values[A_NONE] = -1.0;
  goal_values[A_UNSET] = -1.0;
  goal_values[A_NONE] = -1.0;

  /* Want of the tech + discounted want of those whose path runs
   * through it (stock daitech.c:118, with the exponent ours). */
  advance_index_iterate_max(A_FIRST, i, ac) {
    if (valid_advance_by_number(i)) {
      int steps = research_goal_unknown_techs(presearch, i);

      if (steps > 0) {
        double spread = (double) plr_data->tech_want[i]
                        / pow((double) steps, power);

        values[i] += (double) plr_data->tech_want[i];
        advance_index_iterate_max(A_FIRST, k, ac) {
          if (research_goal_tech_req(presearch, i, k)) {
            values[k] += spread;
          }
        } advance_index_iterate_max_end;
      }
    }
  } advance_index_iterate_max_end;

  /* Goal values: the path's accumulated mass over distance^power
   * (stock daitech.c:135). */
  advance_index_iterate_max(A_FIRST, i, ac) {
    if (valid_advance_by_number(i)) {
      int steps = research_goal_unknown_techs(presearch, i);

      if (steps == 0) {
        goal_values[i] = -1.0;
        continue;
      }

      goal_values[i] = values[i];
      advance_index_iterate_max(A_FIRST, k, ac) {
        if (research_goal_tech_req(presearch, i, k)) {
          goal_values[i] += values[k];
        }
      } advance_index_iterate_max_end;

      goal_values[i] /= pow((double) steps, power);
    } else {
      goal_values[i] = -1.0;
    }
  } advance_index_iterate_max_end;

  newtech = A_UNSET;
  newgoal = A_UNSET;
  advance_index_iterate_max(A_FIRST, i, ac) {
    if (valid_advance_by_number(i)) {
      if (values[i] > values[newtech]
          && research_invention_gettable(presearch, i, TRUE)) {
        newtech = i;
      }
      if (goal_values[i] > goal_values[newgoal]
          && research_invention_reachable(presearch, i)) {
        newgoal = i;
      }
    }
  } advance_index_iterate_max_end;

  /* The goal first: it costs nothing to move and only takes effect
   * when a research completes (techtools.c:505). */
  if (newgoal != presearch->tech_goal) {
    choose_tech_goal(presearch, newgoal);
  }

  /* The researching advance, behind stock's own guard shape
   * (daitech.c:356): the gain must beat the switching penalty, and a
   * nearly-finished research is never abandoned. Recomputed from the
   * post-stock state, which is conservative -- if stock already
   * switched this turn, the penalty it paid is counted against us
   * too. */
  penalty = MAX(presearch->bulbs_researched - presearch->free_bulbs, 0);
  if (newtech != A_UNSET && newtech != presearch->researching) {
    double want = values[newtech] / num_cities_nonzero;
    double current_want = values[presearch->researching]
                          / num_cities_nonzero;

    if (want - current_want > (double) penalty
        && (penalty + presearch->bulbs_researched
            <= research_total_bulbs_required(presearch,
                                             presearch->researching,
                                             FALSE))) {
      Tech_type_id before = presearch->researching;

      choose_tech(presearch, newtech);
      /* choose_tech() silently refuses a tech that is not
       * TECH_PREREQS_KNOWN (techtools.c:1021), and the argmax admits
       * such techs -- its gettable(TRUE) test is only root_reqs_known.
       * Stock trips the same no-op (upstream-bugs.md); switched=
       * reports what actually MOVED, not what was attempted. */
      switched = (presearch->researching != before);
    }
  }

  /* The counter. `stockgoal`/`stockres` are stock's COMMITTED picks
   * from the dai_manage_tech() call that just returned -- not a
   * recomputation -- so a reader can take the differ rate as exactly
   * "how often the reselection moved the decision" (FENCE's
   * would-have pattern). Wants are deep's valuation at `power` of
   * both goals, per (average) city, comparable within the line. */
  log_base(deep_metric_log_level(),
           "deep-techgoal turn=%d player=\"%s\" power=%.3f"
           " goal=\"%s\" goalmil=%d goalsteps=%d goalwant=%.1f"
           " stockgoal=\"%s\" stockgoalmil=%d stockgoalsteps=%d"
           " stockgoalwant=%.1f differ=%d"
           " res=\"%s\" stockres=\"%s\" resdiffer=%d switched=%d",
           game.info.turn, player_name(pplayer), power,
           tg_name(newgoal),
           deep_techcensus_advance_military(newgoal),
           valid_advance_by_number(newgoal)
             ? research_goal_unknown_techs(presearch, newgoal) : -1,
           valid_advance_by_number(newgoal)
             ? goal_values[newgoal] / num_cities_nonzero : -1.0,
           tg_name(stock_goal),
           deep_techcensus_advance_military(stock_goal),
           valid_advance_by_number(stock_goal)
             ? research_goal_unknown_techs(presearch, stock_goal) : -1,
           valid_advance_by_number(stock_goal)
             ? goal_values[stock_goal] / num_cities_nonzero : -1.0,
           newgoal != stock_goal ? 1 : 0,
           tg_name(presearch->researching), tg_name(stock_res),
           presearch->researching != stock_res ? 1 : 0,
           switched ? 1 : 0);
}

/**********************************************************************//**
  Deep's research management: stock's, plus the gated reselection.
  See the header comment for the seam.
**************************************************************************/
void deep_manage_tech(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  dai_manage_tech(ait, pplayer);

  if (tune->techgoal_deep == 0) {
    /* Byte-identity: nothing runs past the stock call. */
    return;
  }

  deep_reselect_research(ait, pplayer, tune->techgoal_distance_power);
}
