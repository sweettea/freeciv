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
           " idle=%d idlecity=%d ops=%d musters=%d wars=%d",
           game.info.turn, player_name(pplayer), mil, att, attidle,
           muster, march, strike, raid, lift, escort, ward, garrison, attack,
           hunt, tasked, moved, fort, idle, incity, ops, musters, wars);
}
