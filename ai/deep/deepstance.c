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

/* common */
#include "game.h"
#include "player.h"

#include "deepai.h"
#include "deeptuning.h"

#include "deepstance.h"

/**********************************************************************//**
  Identity: every axis equal, which is what every gate must read as
  "do nothing at all".
**************************************************************************/
struct deep_stance deep_stance_identity(void)
{
  struct deep_stance st;

  st.econ = 1.0;
  st.mil = 1.0;
  st.cult = 1.0;

  return st;
}

/**********************************************************************//**
  Whether this stance is the one the acceptance gate is written
  against. Only the two live axes are asked about: cult is inert
  (W34 A.1c) and no gate consults it.
**************************************************************************/
bool deep_stance_is_identity(const struct deep_stance *st)
{
  return st->econ == st->mil;
}

/**********************************************************************//**
  PINNED mode: the tuning file clamps every deep seat to one corner.
  This is what makes a tuning block separable -- economy and military
  bid for one shield budget, so an economy optimum found under a
  moving military posture is an optimum for spending a starved budget
  (findings/objective-layer.md section 1). FREE is what ships.
**************************************************************************/
bool deep_stance_pinned(void)
{
  return deep_tuning_get()->stance_forced >= 0;
}

/**********************************************************************//**
  Label for a pinned corner. Logging only.
**************************************************************************/
const char *deep_stance_preset_name(int preset)
{
  switch (preset) {
  case DEEP_STANCE_BALANCED:
    return "balanced";
  case DEEP_STANCE_ECONOMIC:
    return "economic";
  case DEEP_STANCE_MILITARY:
    return "military";
  default:
    return "?";
  }
}

/**********************************************************************//**
  Label for a point on the A.4 ladder (identity -> traits ->
  traits+position). Logging only.
**************************************************************************/
const char *deep_stance_source_name(int source)
{
  switch (source) {
  case 0:
    return "identity";
  case 1:
    return "traits";
  case 2:
    return "position";
  default:
    return "?";
  }
}

/**********************************************************************//**
  The source actually in force, which is not always the one the tuning
  file asked for: sources 1 (traits, W34 A.1a) and 2 (position, A.4
  step 3) are E34.3 and E34.4 and are not built. A file that asks for
  one of them gets identity and says so out loud, once -- a switch
  nobody verified has already cost this project a 40-game batch.
**************************************************************************/
static int deep_stance_source_effective(void)
{
  int want = deep_tuning_get()->stance_source;

  if (want == 0) {
    return 0;
  }
  if (want > 0 && want < 3) {
    static bool warned = FALSE;

    if (!warned) {
      warned = TRUE;
      log_error("deep-stance: stance_source=%d (%s) is not implemented"
                " in this build; using identity", want,
                deep_stance_source_name(want));
    }
  }
  return 0;
}

/**********************************************************************//**
  Set a player's axes for this turn.

  E34.1 resolves exactly two things: IDENTITY (what ships, and what
  the byte-identity gate is taken against) and a PINNED corner. The
  ladder that makes players differ from each other -- traits, then
  position -- lands in E34.3/E34.4 deliberately after every gate has
  an execution counter, so a behaviour change there is attributable to
  the trait mapping rather than to a gate nobody exercised.

  The corner depth is stance_trait_span, shared with the traits step
  on purpose: a pinned corner is then exactly the extreme a
  trait-driven player will be able to reach, so a constant tuned
  pinned means the same thing free.
**************************************************************************/
void deep_stance_refresh(struct player *pplayer)
{
  struct deep_plr *plr_data = deep_plr_data(pplayer);
  const struct deep_tuning *tune = deep_tuning_get();
  double span;

  plr_data->stance = deep_stance_identity();
  (void) deep_stance_source_effective();

  if (!deep_stance_pinned()) {
    return;
  }

  /* Clamped short of 1.0: an axis of zero is a ratio nothing can be
   * read from, and "a high econ weight means minimise military spend
   * SUBJECT TO SURVIVAL, not no army" (W34 A.2). The survival floor
   * is enforced separately -- gate 1 never scales it. */
  span = MIN(MAX(tune->stance_trait_span, 0.0), 0.9);

  switch (tune->stance_forced) {
  case DEEP_STANCE_ECONOMIC:
    plr_data->stance.econ = 1.0 + span;
    plr_data->stance.mil = 1.0 - span;
    break;
  case DEEP_STANCE_MILITARY:
    plr_data->stance.econ = 1.0 - span;
    plr_data->stance.mil = 1.0 + span;
    break;
  case DEEP_STANCE_BALANCED:
  default:
    /* Pinned to identity: a control arm that exercises the pinning
     * path and must still be byte-identical. */
    break;
  }
}

/**********************************************************************//**
  One line per player per turn: the weights themselves, the source and
  the mode.

  The weights are on the line rather than just a preset name because
  the two ways this layer can fail are "the gates are not reading the
  axes" and "the axes never moved", and only the numbers tell them
  apart. The mode is on the line because a tuning result taken FREE
  and reported as PINNED is a wasted campaign (W34 A.1b), and this
  project has already lost a 40-game batch to a switch nobody
  verified: a pinned arm must show one distinct stance across all
  seats, a free arm four.
**************************************************************************/
void deep_stance_log(struct player *pplayer)
{
  const struct deep_plr *plr_data = deep_plr_data(pplayer);
  const struct deep_tuning *tune = deep_tuning_get();
  bool pinned = deep_stance_pinned();

  log_base(deep_metric_log_level(),
           "deep-stance turn=%d player=\"%s\" econ=%.3f mil=%.3f"
           " cult=%.3f src=%s mode=%s preset=%s identity=%d"
           " fear_gate=%d op_gate=%d growth_gate=%d expand_gate=%d",
           game.info.turn, player_name(pplayer),
           plr_data->stance.econ, plr_data->stance.mil,
           plr_data->stance.cult,
           deep_stance_source_name(deep_stance_source_effective()),
           pinned ? "PINNED" : "FREE",
           pinned ? deep_stance_preset_name(tune->stance_forced) : "-",
           deep_stance_is_identity(&plr_data->stance) ? 1 : 0,
           /* Cumulative, so the last line of a game is the total and
            * a difference between consecutive lines is the turn's
            * firing rate. Zero on every line is the identity arm's
            * proof that the gate is inert, not merely quiet. */
           plr_data->stance_fear_gated,
           /* One field per gate on ONE line, so a single grep answers
            * "did anything execute" for the whole layer. */
           plr_data->stance_op_gated,
           plr_data->stance_growth_gated,
           plr_data->stance_expand_gated);
}

/**********************************************************************//**
  Gate 1 (W34 A.2): the ANTICIPATORY fear terms are scaled by the
  military-to-economic ratio, so a player whose weight is on its
  economy is frightened less by what it cannot yet see.

  Shape: (mil / econ) ^ stance_fear_gain. It is EXACTLY 1.0 when the
  axes are equal, whatever the gain, which is what makes the
  byte-identity gate provable rather than argued -- x * 1.0 == x for
  every finite x in IEEE 754.

  What it does NOT scale is the survival floor: the sequential-
  attrition deficit (deep_survival_deficit_fear) and the garrison
  floor both price OBSERVED threats and keep full weight. Gate 1
  scales anticipation TOWARD that floor, never below it -- "a high
  econ weight means minimise military spend subject to survival, not
  no army".
**************************************************************************/
double deep_stance_fear_mult(const struct player *pplayer)
{
  const struct deep_plr *plr_data = deep_plr_data(pplayer);
  const struct deep_stance *st;
  const struct deep_tuning *tune = deep_tuning_get();

  if (plr_data == NULL) {
    return 1.0;
  }
  st = &plr_data->stance;
  if (st->econ <= 0.0 || st->mil <= 0.0
      || deep_stance_is_identity(st)
      || tune->stance_fear_gain == 0.0) {
    return 1.0;
  }

  return pow(st->mil / st->econ, tune->stance_fear_gain);
}

/**********************************************************************//**
  The shape every gate after the first shares: (econ / mil) ^ gain.

  Gate 1 reads the ratio the other way round because fear is the one
  quantity a MILITARY weight should raise; gates 2-4 all price things
  an ECONOMIC weight should raise (or, for the formation bar, should
  make harder to clear). Exactly 1.0 whenever the axes are equal,
  whatever the gain, which is what the byte-identity gate rests on:
  x * 1.0 == x for every finite x in IEEE 754.

  A zero gain disables its gate outright without touching the axes,
  which is the per-gate off-switch an A/B needs.
**************************************************************************/
static double deep_stance_econ_mult(const struct player *pplayer,
                                    double gain)
{
  const struct deep_plr *plr_data = deep_plr_data(pplayer);
  const struct deep_stance *st;

  if (plr_data == NULL) {
    return 1.0;
  }
  st = &plr_data->stance;
  if (st->econ <= 0.0 || st->mil <= 0.0
      || deep_stance_is_identity(st)
      || gain == 0.0) {
    return 1.0;
  }

  return pow(st->econ / st->mil, gain);
}

/**********************************************************************//**
  Gate 2 (W34 A.2): the multiplier on the ODDS BAR an assault must
  clear before an operation forms. Above 1.0 -- an economy-weighted
  player -- the bar rises toward certainty, so blades are committed
  only where the take is near-certain; below 1.0 the bar falls and
  operations form freely. The caller caps the scaled bar at 1.0 and
  exempts targets that threaten us: that exemption is gate 1's
  survival-floor precedent pointed outward.
**************************************************************************/
double deep_stance_op_mult(const struct player *pplayer)
{
  return deep_stance_econ_mult(pplayer, deep_tuning_get()->stance_op_gain);
}

/**********************************************************************//**
  Gate 3 (W34 A.2): the multiplier on the horizon growth credits --
  the growth ladder that bids against barracks and walls for the same
  shields. The credits are additive on top of stock's own base want
  and are floored at zero by their caller, so this gate can raise what
  growth is worth to this player and can withdraw deep's credit, but
  it can never price a building below stock's valuation of it.
**************************************************************************/
double deep_stance_growth_mult(const struct player *pplayer)
{
  return deep_stance_econ_mult(pplayer,
                               deep_tuning_get()->stance_growth_gain);
}

/**********************************************************************//**
  Gate 4 (W34 A.2): the multiplier on EXPANSION -- the settler want
  and the colony lift that carries it across water. "High econ expands
  WHILE LAND REMAINS": whether land remains is already asked, by the
  site machinery that produced the want in the first place, so this
  gate says only how much this player wants what is left. It does not
  scale the military lift demand that shares the same want site.
**************************************************************************/
double deep_stance_expand_mult(const struct player *pplayer)
{
  return deep_stance_econ_mult(pplayer,
                               deep_tuning_get()->stance_expand_gain);
}
