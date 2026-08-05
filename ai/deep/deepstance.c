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
#include "nation.h"
#include "player.h"
#include "traits.h"

/* ai */
#include "aitraits.h"

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
  file asked for: source 2 (position, A.4 step 3) is E34.4 and is not
  built. A file that asks for it gets identity and says so out loud,
  once -- a switch nobody verified has already cost this project a
  40-game batch.
**************************************************************************/
static int deep_stance_source_effective(void)
{
  int want = deep_tuning_get()->stance_source;

  if (want == 0 || want == 1) {
    return want;
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
  Where one trait sits inside the range its RULESET allows, mapped to
  [-1, +1]: -1 at the nation's minimum for that trait, +1 at its
  maximum, 0 at the midpoint.

  The value itself is read with ai_trait_get_value() -- the same
  accessor the stock AI uses at all seven of its trait sites, so the
  preference source is READ rather than invented (W34 A.1a).

  The normalisation is against the nation's own [min, max] rather than
  against TRAIT_DEFAULT_VALUE (stock's divisor) for one reason that is
  about measurement rather than taste: E34.1 pinned the corners at
  (1 +- stance_trait_span) and recorded that a pinned corner must be
  exactly the extreme a trait-driven player can reach, so that a
  constant tuned pinned means the same thing free. Only a ruleset-
  relative index has that property; dividing by 50 would make the span
  mean a different amount of differentiation in every ruleset, and
  would leave the corner unreachable in all of them.

  A ruleset that gives a trait no range at all (min == max, which is
  what civ1/civ2 do) returns 0: there is nothing here to read, and the
  player stays at identity rather than being assigned a personality we
  made up.
**************************************************************************/
static double deep_stance_trait_index(struct player *pplayer,
                                      enum trait tr)
{
  const struct trait_limits *lim;
  int val, min, max;

  if (pplayer->nation == NULL
      || pplayer->nation->server.traits == NULL) {
    return 0.0;
  }
  lim = &pplayer->nation->server.traits[tr];
  min = lim->min;
  max = lim->max;
  if (max <= min) {
    return 0.0;
  }

  /* The accessor adds the scripted modifier and clips to
   * [1, TRAIT_MAX_VALUE], so a Lua-modified trait can sit outside the
   * ruleset's range; clip it back rather than letting one script push
   * an axis past the corner. */
  val = CLIP(min, ai_trait_get_value(tr, pplayer), max);

  return ((double)(val - min) / (double)(max - min)) * 2.0 - 1.0;
}

/**********************************************************************//**
  Source 1 (W34 A.1a): the axes seeded from this player's TRAITS.

      econ <- EXPANSIONIST / BUILDER / TRADER
      mil  <- AGGRESSIVE
      cult <- no trait exists; stays inert (A.1c)

  Each axis is 1.0 + span * (mean trait index), so a player at every
  relevant maximum with the opposite trait at its minimum reaches
  exactly the corner stance_forced pins to, and a player at the
  midpoint of every trait is exactly identity.

  The two axes are INDEPENDENT rather than two ends of one slider,
  which is what "axes, not a menu" means: an aggressive builder gets
  both weights raised, the gates read the ratio, and it plays today's
  game -- which is right, because nothing about that player prefers
  one budget to the other.

  The military index is summed three times rather than used directly
  so that four traits at the same relative position give EXACTLY equal
  axes: 2x is exact in IEEE 754, so (x + x) + x and 3 * x round
  identically, and the identity test (econ == mil) is then a real test
  rather than one that a rounding difference of one ulp can fail. It
  matters because the shipping rulesets set every trait's default to
  the same value: under 'traitdistribution FIXED' every player draws
  the same four numbers and this source must therefore be inert.
**************************************************************************/
static void deep_stance_seed_from_traits(struct player *pplayer,
                                         double span,
                                         struct deep_stance *st)
{
  double agg = deep_stance_trait_index(pplayer, TRAIT_AGGRESSIVE);
  double esum = deep_stance_trait_index(pplayer, TRAIT_EXPANSIONIST)
    + deep_stance_trait_index(pplayer, TRAIT_BUILDER)
    + deep_stance_trait_index(pplayer, TRAIT_TRADER);
  double msum = agg + agg + agg;

  st->econ = 1.0 + span * (esum / 3.0);
  st->mil = 1.0 + span * (msum / 3.0);
}

/**********************************************************************//**
  Set a player's axes for this turn.

  Three things resolve here, in the order A.4 lands them: IDENTITY
  (what ships, and what the byte-identity gate is taken against),
  TRAITS (source 1, E34.3), and a PINNED corner, which overrides both
  because a pin is what makes a tuning block separable.

  The corner depth is stance_trait_span, shared with the traits step
  on purpose: a pinned corner is exactly the extreme a trait-driven
  player can reach, so a constant tuned pinned means the same thing
  free.
**************************************************************************/
void deep_stance_refresh(struct player *pplayer)
{
  struct deep_plr *plr_data = deep_plr_data(pplayer);
  const struct deep_tuning *tune = deep_tuning_get();
  double span;

  plr_data->stance = deep_stance_identity();

  /* Clamped short of 1.0: an axis of zero is a ratio nothing can be
   * read from, and "a high econ weight means minimise military spend
   * SUBJECT TO SURVIVAL, not no army" (W34 A.2). The survival floor
   * is enforced separately -- gate 1 never scales it. */
  span = MIN(MAX(tune->stance_trait_span, 0.0), 0.9);

  if (!deep_stance_pinned()) {
    if (deep_stance_source_effective() == 1) {
      deep_stance_seed_from_traits(pplayer, span, &plr_data->stance);
    }
    return;
  }

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
           " fear_gate=%d op_gate=%d growth_gate=%d expand_gate=%d"
           " assault_gate=%d",
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
           plr_data->stance_expand_gated,
           plr_data->stance_assault_gated);
}

/**********************************************************************//**
  The shape gates 1 and 5 share: (mil / econ) ^ gain.

  These two price MILITARY wants -- what a threat is worth being
  frightened of, and what a target is worth buying blades for -- so an
  economic weight must LOWER them. Gates 2-4 price things an economic
  weight raises and read the ratio the other way up. Exactly 1.0
  whenever the axes are equal, whatever the gain, which is what the
  byte-identity gate rests on: x * 1.0 == x for every finite x in
  IEEE 754. A zero gain disables its gate outright without touching
  the axes, which is the per-gate off-switch an A/B needs.
**************************************************************************/
static double deep_stance_mil_mult(const struct player *pplayer,
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

  return pow(st->mil / st->econ, gain);
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
  return deep_stance_mil_mult(pplayer,
                              deep_tuning_get()->stance_fear_gain);
}

/**********************************************************************//**
  Gate 5 (W34 A.2, added in E34.2b): the multiplier on the OFFENSIVE
  BUILD -- deep_assault_want()'s marginal-take-odds attacker demand,
  the want that turns a target's worth into shields spent on blades.

  E34.2's regime check failed with offence share RISING under an
  economic pin, and the census named why: gate 2 governs whether
  blades that already EXIST are committed, and no gate priced buying
  them, so the defenders gate 1 discouraged simply stopped competing
  for the slot. This is that gate. It reads (mil/econ)^gain, gate 1's
  direction, because it prices a military want.

  Its floor is gate 2's, and deliberately the SAME test rather than a
  second one: a target that THREATENS us -- a barbarian holding, or a
  hostile city on a landmass where we hold cities -- keeps the
  unscaled want. Reconquest of what menaces us is survival, not
  ambition, and "minimise military spend SUBJECT TO SURVIVAL" has to
  mean that on the offensive side too. The caller owns the exemption
  because only it knows the target.
**************************************************************************/
double deep_stance_assault_mult(const struct player *pplayer)
{
  return deep_stance_mil_mult(pplayer,
                              deep_tuning_get()->stance_assault_gain);
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
