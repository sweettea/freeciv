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

#include <stdlib.h>
#include <string.h>

/* utility */
#include "log.h"

/* common */
#include "city.h"
#include "game.h"
#include "player.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

#include "deepdefence.h"
#include "deeptuning.h"

#include "deepexpand.h"

/* One tally per player slot, reset when its line is printed. Player
 * slots rather than a list, for the same reason deepfort's rings are:
 * the notes arrive from five call sites that all have a player and none
 * of which owns storage. */
struct deep_expand_tally {
  int cities;
  int gate;
  int norole;
  int fresh, stale;
  int w0, wpos, wneg;
  int held, vfood, vwonder, vcap;
  int skipwar, skipemerg;
  int contest, contwin;
  int hold, holdyld;
  int auc, win, lostdom, lostmil, lostmilt;
  /* The magnitudes behind the counts: what a founder bid was worth on
   * the city-turns where it existed, and on the ones where it lost. */
  double wantsum, bidsum, lostsum;
};

static struct deep_expand_tally deep_expand_t[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  This player's tally.
**************************************************************************/
static struct deep_expand_tally *deep_expand_of(const struct player *pplayer)
{
  return &deep_expand_t[player_index(pplayer)];
}

/**********************************************************************//**
  The expansion gate's verdict at its call site. Counted only when it
  actually zeroed the want -- the arm where the gate fires against a
  city with no military want changes nothing and is already reported by
  deep-expgate's own line.
**************************************************************************/
void deep_expand_note_gate(const struct player *pplayer, bool effective)
{
  struct deep_expand_tally *t = deep_expand_of(pplayer);

  t->cities++;
  if (effective) {
    t->gate++;
  }
}

/**********************************************************************//**
  No ACTION_FOUND_CITY role unit exists, so contemplation returned
  without touching the want.
**************************************************************************/
void deep_expand_note_norole(const struct player *pplayer)
{
  deep_expand_of(pplayer)->norole++;
}

/**********************************************************************//**
  What the contemplation step left in founder_want, after deep's own
  rush and stance multipliers have had it.
**************************************************************************/
void deep_expand_note_want(const struct player *pplayer, bool fresh,
                           int want)
{
  struct deep_expand_tally *t = deep_expand_of(pplayer);

  if (fresh) {
    t->fresh++;
    if (want > 0) {
      t->wpos++;
    } else if (want < 0) {
      t->wneg++;
    } else {
      t->w0++;
    }
  } else {
    t->stale++;
  }
  t->wantsum += abs(want);
}

/**********************************************************************//**
  The bid seam. `fed` is daidomestic's food veto: a city whose food
  surplus is below the founder's upkeep makes no founder bid whatever,
  at any want, so a want that dies here never reaches an auction at all.
**************************************************************************/
void deep_expand_note_bid(const struct player *pplayer, int want,
                          bool wonder, bool capped, bool fed)
{
  struct deep_expand_tally *t = deep_expand_of(pplayer);

  if (want == 0) {
    return;
  }
  t->held++;
  t->bidsum += abs(want);
  if (!fed) {
    t->vfood++;
  }
  if (wonder) {
    t->vwonder++;
  }
  if (capped) {
    t->vcap++;
  }
}

/**********************************************************************//**
  The domestic advisor was not called for this city-turn, so no founder
  bid was made however large the want. Inherited from
  dai_city_choose_build(); `war` separates the war-footing arm from the
  emergency-tier arm.
**************************************************************************/
void deep_expand_note_skip(const struct player *pplayer, bool war)
{
  struct deep_expand_tally *t = deep_expand_of(pplayer);

  if (war) {
    t->skipwar++;
  } else {
    t->skipemerg++;
  }
}

/**********************************************************************//**
  founder_tier_gate raised a contest for this city-turn: deep's own
  founder want strictly outbid the military want on magnitude, so the
  emergency skip is not taken and a founder that wins the domestic
  advisor's own auction is exempt from the tier at the merge. Zero
  with the gate off, which is what makes the funnel line the gate's
  liveness proof.
**************************************************************************/
void deep_expand_note_contest(const struct player *pplayer)
{
  deep_expand_of(pplayer)->contest++;
}

/**********************************************************************//**
  ... and the contest ended with a founder taking the build.
**************************************************************************/
void deep_expand_note_contest_win(const struct player *pplayer)
{
  deep_expand_of(pplayer)->contwin++;
}

/**********************************************************************//**
  founder_run_hold declined a military switch away from a founder the
  city is already building. Zero with the gate off.
**************************************************************************/
void deep_expand_note_hold(const struct player *pplayer)
{
  deep_expand_of(pplayer)->hold++;
}

/**********************************************************************//**
  ... and the same run, released because the advisor raised
  grave_danger. The exception's own counter: a large number here means
  the hold is being overruled rather than binding.
**************************************************************************/
void deep_expand_note_hold_yield(const struct player *pplayer)
{
  deep_expand_of(pplayer)->holdyld++;
}

/**********************************************************************//**
  The auction settled with a founder bid in hand. Three outcomes, each
  charged where it happened: the domestic advisor preferred something
  else, the merge preferred the military choice, or the founder won.

  `lostmilt` records the third case with the military side in the
  EMERGENCY tier. It was introduced as the TIER half of that case, on
  the reading that deep's (tier, value) order (deepwant.c) can refuse
  a founder want that stock's magnitude order would have seated. It
  cannot: domestic_advisor_choose_build() clamps whatever it returns
  to DAI_WANT_DOMESTIC_MAX (daidomestic.c:631 = 750.075) and the
  emergency tier starts at 1000, so a founder bid ARRIVES at the merge
  already below any emergency want, and deep_want_from_choice() is
  monotone in the raw want besides -- the two orders agree on every
  comparison that reaches this seam. Every lostmilt is a magnitude
  loss as well. The comparison that can distinguish them happens one
  stage earlier and is founder_tier_gate's (deepdefence.c).
**************************************************************************/
void deep_expand_note_auction(const struct player *pplayer, int founder_bid,
                              bool dom_founder, bool dom_won,
                              bool mil_emergency)
{
  struct deep_expand_tally *t = deep_expand_of(pplayer);

  if (founder_bid == 0) {
    return;
  }
  t->auc++;
  if (dom_founder && dom_won) {
    t->win++;
    return;
  }
  t->lostsum += abs(founder_bid);
  if (!dom_founder) {
    t->lostdom++;
  } else {
    t->lostmil++;
    if (mil_emergency) {
      t->lostmilt++;
    }
  }
}

/**********************************************************************//**
  Print this player's funnel line and reset the tally.

  The tail is read off the world rather than hooked, because every part
  of it is a field the server already maintains: a founder's birth turn,
  a city's founding turn, and what each city is building right now. The
  opportunity terms are the cached per-player figures deepdefence
  already computes for the density prior and the founder pool, so
  reading them here costs a lookup and gives a gated city-turn -- one
  that never ran a site search and so emitted no deep-siteladder line --
  a statement about the land it declined.
**************************************************************************/
void deep_expand_census(struct player *pplayer)
{
  struct deep_expand_tally *t = deep_expand_of(pplayer);
  int born = 0, alive = 0, found = 0, prod = 0;

  unit_list_iterate(pplayer->units, punit) {
    if (unit_is_cityfounder(punit)) {
      alive++;
      if (punit->birth_turn == game.info.turn) {
        born++;
      }
    }
  } unit_list_iterate_end;

  city_list_iterate(pplayer->cities, pcity) {
    if (pcity->turn_founded == game.info.turn) {
      found++;
    }
    if (VUT_UTYPE == pcity->production.kind
        && utype_is_cityfounder(pcity->production.value.utype)) {
      prod++;
    }
  } city_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-expand turn=%d player=\"%s\" cities=%d gate=%d norole=%d"
           " fresh=%d stale=%d w0=%d wpos=%d wneg=%d held=%d vfood=%d"
           " vwonder=%d vcap=%d skipwar=%d skipemerg=%d contest=%d"
           " contwin=%d hold=%d holdyld=%d auc=%d win=%d"
           " lostdom=%d lostmil=%d lostmilt=%d wantsum=%.0f bidsum=%.0f"
           " lostsum=%.0f prod=%d born=%d alive=%d found=%d sites=%.2f"
           " freetiles=%d freeshare=%.3f",
           game.info.turn, player_name(pplayer),
           t->cities, t->gate, t->norole, t->fresh, t->stale,
           t->w0, t->wpos, t->wneg, t->held, t->vfood, t->vwonder, t->vcap,
           t->skipwar, t->skipemerg, t->contest, t->contwin,
           t->hold, t->holdyld, t->auc, t->win, t->lostdom,
           t->lostmil, t->lostmilt, t->wantsum, t->bidsum, t->lostsum,
           prod, born, alive, found,
           deep_remaining_sites(pplayer),
           deep_reachable_free_land(pplayer),
           deep_free_land_share(pplayer));

  memset(t, 0, sizeof(*t));
}
