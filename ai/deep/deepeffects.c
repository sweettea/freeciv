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

/* common */
#include "city.h"
#include "effects.h"
#include "improvement.h"
#include "multipliers.h"
#include "player.h"
#include "requirements.h"

#include "deepeffects.h"

/**********************************************************************//**
  Would building this improvement, by itself, satisfy this requirement's
  source?

  Two ways it can, and the stock accessor knows only the first:

  1. The requirement names the improvement (VUT_IMPROVEMENT). This is
     stock's own case, reproduced exactly.
  2. The requirement names a building FLAG the improvement carries
     (VUT_IMPR_FLAG). A city-range flag requirement asks whether some
     building here has the flag; the candidate is precisely a building
     that would have it, so the question the credit is asking -- "what
     would I get if I built this" -- is answered TRUE by the candidate
     for the same reason case 1 is.

  The answer is about the SOURCE only. The caller reads `preq->present`
  itself, so an effect the candidate would switch OFF (a "must not have
  flag X" requirement, with a candidate that has X) is charged as a loss
  exactly the way stock charges a "must not have building Y" one.
**************************************************************************/
bool deep_req_source_is_candidate(const struct requirement *preq,
                                  const struct impr_type *pimprove)
{
  switch (preq->source.kind) {
  case VUT_IMPROVEMENT:
    return preq->source.value.building == pimprove;
  case VUT_IMPR_FLAG:
    return improvement_has_flag(pimprove, preq->source.value.impr_flag);
  default:
    return FALSE;
  }
}

/**********************************************************************//**
  The effect bonus the improvement would or does provide if present,
  evaluated against a context that knows the candidate's flags.

  See deepeffects.h for why deep does not simply call
  get_potential_improvement_bonus(). The loop below is that function's,
  with deep_req_source_is_candidate() in place of its inline
  VUT_IMPROVEMENT test; everything else -- the effect index, the
  requirement evaluator, the problem-type discipline, the multiplier
  handling -- is common/'s own code called from here.
**************************************************************************/
int deep_potential_improvement_bonus(const struct impr_type *pimprove,
                                     const struct city *pcity,
                                     enum effect_type effect_type,
                                     const enum req_problem_type prob_type,
                                     bool consider_multipliers)
{
  struct universal source = { .kind = VUT_IMPROVEMENT,
                              .value = {.building = pimprove}};
  struct effect_list *plist = get_req_source_effects(&source);
  struct player *owner = nullptr;
  int power = 0;

  if (plist == nullptr) {
    return 0;
  }

  if (pcity != nullptr) {
    owner = city_owner(pcity);
  }

  {
    const struct req_context context = {
      .player = owner,
      .city = pcity,
      .building = pimprove,
    };

    effect_list_iterate(plist, peffect) {
      bool present = TRUE;
      bool useful = TRUE;

      if (peffect->type != effect_type) {
        continue;
      }

      requirement_vector_iterate(&peffect->reqs, preq) {
        if (deep_req_source_is_candidate(preq, pimprove)) {
          present = preq->present;
          continue;
        }

        if (!is_req_active(&context, nullptr, preq, prob_type)) {
          useful = FALSE;
          break;
        }
      } requirement_vector_iterate_end;

      if (useful) {
        int val;

        if (present) {
          val = peffect->value;
        } else {
          val = -peffect->value;
        }

        if (consider_multipliers && peffect->multiplier) {
          val = val * player_multiplier_effect_value(owner,
                                                     peffect->multiplier) / 100;
        }

        power += val;
      }
    } effect_list_iterate_end;
  }

  return power;
}
