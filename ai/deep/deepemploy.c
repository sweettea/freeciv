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
#include "log.h"
#include "mem.h"
#include "support.h"

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server/advisors */
#include "advgoto.h"

/* ai/default */
#include "daiplayer.h"
#include "daiunit.h"

#include "deepai.h"
#include "deepops.h"
#include "deeptuning.h"

#include "deepemploy.h"

/* The formation funnel, cumulative over the game, plus the one field
 * that is not cumulative: this turn's verdict, which the disposition
 * census reads and the wait ledger slices by. `verdict_turn` is what
 * makes that field turn-scoped without a reset pass -- a stale value
 * from an earlier turn reads as DEV_NOWAR, which is exactly what "no
 * formation attempt happened this turn" means. */
struct deep_employ_plr {
  int attempts;
  int verdict[DEV_VERDICT_COUNT];
  int priced;                   /* Attempts that reached the odds loop. */
  int odds_zero;                /* Of those, ones that priced at 0.000. */
  int odds_short;               /* Priced above zero, below the bar. */
  int odds_clear;               /* Cleared the bar -- a real quorum. */
  int pool_sum;                 /* Free blades summed over `priced`. */
  int draft_fired;              /* Attempts where the garrison draft gave. */
  int drafted;                  /* Blades it gave, summed. */
  double odds_sum;
  double bar_sum;

  enum deep_employ_verdict verdict_now;
  int verdict_turn;
};

static struct deep_employ_plr *deep_employ_plr_tbl[MAX_NUM_PLAYER_SLOTS];

static const char *const deep_employ_verdict_name[DEV_VERDICT_COUNT] = {
  "nowar", "noslot", "nohome", "notarget", "nostaging", "poolempty",
  "musteroff", "worth", "muster", "formed"
};

/**********************************************************************//**
  This player's ledger, created on demand. NULL when the metric lines
  are not being written: the whole ledger is instrumentation, so when
  nothing will read it, nothing is kept and nothing is spent. Same
  contract as deep_funnel_get().
**************************************************************************/
static struct deep_employ_plr *deep_employ_get(const struct player *pplayer,
                                               bool create)
{
  int pn = player_number(pplayer);
  struct deep_employ_plr *ep;

  if (pn < 0 || pn >= MAX_NUM_PLAYER_SLOTS) {
    return NULL;
  }
  ep = deep_employ_plr_tbl[pn];
  if (ep != NULL || !create
      || !log_do_output_for_level(deep_metric_log_level())) {
    return ep;
  }
  ep = fc_calloc(1, sizeof(*ep));
  deep_employ_plr_tbl[pn] = ep;

  return ep;
}

/**********************************************************************//**
  One formation attempt begins.
**************************************************************************/
void deep_employ_form_attempt(const struct player *pplayer)
{
  struct deep_employ_plr *ep = deep_employ_get(pplayer, TRUE);

  if (ep != NULL) {
    ep->attempts++;
  }
}

/**********************************************************************//**
  What the odds loop saw: the best take-probability any prefix of the
  pool reached, the bar it was compared against, the pool it had and
  what the garrison draft added to it. Called once per attempt that got
  as far as pricing -- after the draft, so `pool` is the free pool and
  `drafted` the blades borrowed from the walls on top of it.
**************************************************************************/
void deep_employ_form_priced(const struct player *pplayer, double odds,
                             double bar, int pool, int drafted)
{
  struct deep_employ_plr *ep = deep_employ_get(pplayer, TRUE);

  if (ep == NULL) {
    return;
  }
  ep->priced++;
  ep->odds_sum += odds;
  ep->bar_sum += bar;
  ep->pool_sum += pool;
  if (odds >= bar) {
    ep->odds_clear++;
  } else if (odds > 0.0) {
    ep->odds_short++;
  } else {
    ep->odds_zero++;
  }
  if (drafted > 0) {
    ep->draft_fired++;
    ep->drafted += drafted;
  }
}

/**********************************************************************//**
  The attempt ends here, with this verdict.
**************************************************************************/
void deep_employ_form_verdict(const struct player *pplayer,
                              enum deep_employ_verdict verdict)
{
  struct deep_employ_plr *ep = deep_employ_get(pplayer, TRUE);

  if (ep == NULL || verdict < 0 || verdict >= DEV_VERDICT_COUNT) {
    return;
  }
  ep->verdict[verdict]++;
  ep->verdict_now = verdict;
  ep->verdict_turn = game.info.turn;
}

/**********************************************************************//**
  The cumulative funnel line, printed beside the census. Cumulative for
  the same reason deepfunnel's is: "what became of a formation attempt"
  cannot be answered by pooling per-turn medians, and the LAST such line
  of a game is that game's funnel.
**************************************************************************/
static void deep_employ_form_census(struct player *pplayer)
{
  struct deep_employ_plr *ep = deep_employ_get(pplayer, FALSE);
  char verdicts[DEV_VERDICT_COUNT * 24];
  int i, at = 0;

  if (ep == NULL || ep->attempts <= 0) {
    return;
  }
  for (i = 0; i < DEV_VERDICT_COUNT; i++) {
    at += fc_snprintf(verdicts + at, sizeof(verdicts) - at, "%s%s=%d",
                      i == 0 ? "" : ",", deep_employ_verdict_name[i],
                      ep->verdict[i]);
  }

  log_base(deep_metric_log_level(),
           "deep-employform turn=%d player=\"%s\" attempts=%d priced=%d"
           " oddszero=%d oddsshort=%d oddsclear=%d pool=%d draftfired=%d"
           " drafted=%d moddsx1000=%d mbarx1000=%d %s",
           game.info.turn, player_name(pplayer), ep->attempts, ep->priced,
           ep->odds_zero, ep->odds_short, ep->odds_clear, ep->pool_sum,
           ep->draft_fired, ep->drafted,
           ep->priced > 0 ? (int) (1000.0 * ep->odds_sum / ep->priced) : 0,
           ep->priced > 0 ? (int) (1000.0 * ep->bar_sum / ep->priced) : 0,
           verdicts);
}

/**********************************************************************//**
  The disposition census: every military unit of this player in exactly
  one state, once per turn.

  The states are ordered by CLAIM STRENGTH, and first match wins, so the
  line partitions the army rather than describing it: a marching op
  member with a bodyguard charge is counted as marching, because the
  operation is what decides its turn.

    muster    claimed by an assault still gathering at its staging city
    march     claimed by an assault walking to the target
    strike    claimed by an assault at the walls or holding what it took
    raid/lift claimed by the hunt or by a crossing
    garrison  AIUNIT_DEFEND_HOME -- the walls, by task
    attack    AIUNIT_ATTACK on stock's leash (not op-claimed)
    hunt      AIUNIT_HUNTER
    escort    AIUNIT_ESCORT -- bodyguarding
    tasked    any other non-NONE stock task
    ward      no task at all, but still holding a charge
    moved     unassigned, but it spent movement this turn
    fort      unassigned, fortified or fortifying, full moves
    idle      unassigned, full moves, not even fortified

  `att` counts those with a positive attack rating -- the offensive
  denominator, which is the one the kills/built ratio is really about --
  and `attidle` the ones of those that ended the turn in `fort` or
  `idle`. An army that is 1.06x the size and idle-heavy in `attidle` is
  a different finding from one that is idle-heavy in `garrison`.

  Pure reads throughout: unit_move_rate(), the ai task, the op claim
  registry and the activity are all already-computed state.
**************************************************************************/
void deep_employ_census(struct ai_type *ait, struct player *pplayer)
{
  int mil = 0, att = 0, attidle = 0;
  int muster = 0, march = 0, strike = 0, raid = 0, lift = 0;
  int escort = 0, ward = 0, garrison = 0, attack = 0, hunt = 0, tasked = 0;
  int moved = 0, fort = 0, idle = 0;
  int incity = 0, wars = 0, ops = 0, musters = 0;
  const struct deep_employ_plr *ep = deep_employ_get(pplayer, FALSE);
  enum deep_employ_verdict verdict
    = (ep != NULL && ep->verdict_turn == game.info.turn)
      ? ep->verdict_now : DEV_NOWAR;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  players_iterate_alive(aplayer) {
    if (aplayer != pplayer && pplayers_at_war(pplayer, aplayer)
        && player_diplstate_get(pplayer, aplayer)->type != DS_NO_CONTACT) {
      wars++;
    }
  } players_iterate_alive_end;

  {
    const struct deep_plr *dp = deep_plr_data(pplayer);
    int oi;

    for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
      if (dp->ops[oi].state == DEEP_OP_NONE) {
        continue;
      }
      if (dp->ops[oi].kind == DEEP_OPK_ASSAULT) {
        ops++;
        if (dp->ops[oi].muster) {
          musters++;
        }
      }
    }
  }

  unit_list_iterate(pplayer->units, punit) {
    const struct unit_ai *ud = def_ai_unit_data(punit, ait);
    const struct deep_plr *dp = deep_plr_data(pplayer);
    int oi;
    bool still;
    int *bucket = NULL;

    if (punit->hp <= 0 || unit_has_type_flag(punit, UTYF_CIVILIAN)) {
      continue;
    }
    mil++;
    if (adv_unit_att_rating(punit) > 0) {
      att++;
    }
    still = (punit->moves_left >= unit_move_rate(punit));

    oi = deep_op_claiming(pplayer, punit->id);
    if (oi >= 0) {
      const struct deep_operation *op = &dp->ops[oi];

      if (op->kind == DEEP_OPK_RAID) {
        bucket = &raid;
      } else if (op->kind == DEEP_OPK_LIFT) {
        bucket = &lift;
      } else {
        switch (op->state) {
        case DEEP_OP_MUSTER: bucket = &muster; break;
        case DEEP_OP_MARCH:  bucket = &march;  break;
        default:             bucket = &strike; break;
        }
      }
    } else {
      switch (ud->task) {
      case AIUNIT_DEFEND_HOME: bucket = &garrison; break;
      case AIUNIT_ATTACK:      bucket = &attack;   break;
      case AIUNIT_HUNTER:      bucket = &hunt;     break;
      case AIUNIT_ESCORT:      bucket = &escort;   break;
      case AIUNIT_NONE:
        /* `charge` alone does not mean bodyguard: dai_unit_new_task()
         * calls aiguard_assign_guard_city() for every DEFEND_HOME
         * assignment, so a city garrison carries a charge too. Read the
         * task first and the charge only where the task says nothing --
         * a unit holding a charge with no task is a guard whose ward
         * has been re-tasked out from under it. */
        if (ud->charge > 0) {
          bucket = &ward;
        } else if (!still) {
          bucket = &moved;
        } else if (punit->activity == ACTIVITY_FORTIFIED
                   || punit->activity == ACTIVITY_FORTIFYING) {
          bucket = &fort;
        } else {
          bucket = &idle;
        }
        break;
      default:
        bucket = &tasked;
        break;
      }
    }
    (*bucket)++;
    if ((bucket == &fort || bucket == &idle)) {
      if (adv_unit_att_rating(punit) > 0) {
        attidle++;
      }
      if (tile_city(unit_tile(punit)) != NULL) {
        incity++;
      }
    }
  } unit_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-employ turn=%d player=\"%s\" mil=%d att=%d attidle=%d"
           " muster=%d march=%d strike=%d raid=%d lift=%d escort=%d"
           " ward=%d garrison=%d attack=%d hunt=%d tasked=%d moved=%d fort=%d"
           " idle=%d idlecity=%d ops=%d musters=%d wars=%d why=%s",
           game.info.turn, player_name(pplayer), mil, att, attidle,
           muster, march, strike, raid, lift, escort, ward, garrison, attack,
           hunt, tasked, moved, fort, idle, incity, ops, musters, wars,
           deep_employ_verdict_name[verdict]);

  deep_employ_form_census(pplayer);
}
