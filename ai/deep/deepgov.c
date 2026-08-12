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
#include "government.h"
#include "player.h"

/* ai/default */
#include "daidata.h"

#include "deepgov.h"

/* Government valuation (seam map section 4; ladder row 6a).
 *
 * `gov_value` is a vtable hook, so owning the decision costs no
 * `ai/default` change at all -- deep has simply never used the seam.
 * This file is where deep answers it. Until `gov_valuation` moves off
 * its default the answer is still stock's, byte for byte: the point
 * of the first step is that the hook now has an owner, not that the
 * owner has an opinion.
 */

/**********************************************************************//**
  Value of `gov` to `pplayer`, in the advisor's want currency.

  The single caller is `adv_best_government()` (advdata.c), once per
  candidate government per re-evaluation burst. `*override` tells the
  caller whether to skip its own valuation; forwarding to stock leaves
  that contract exactly as it was.
**************************************************************************/
void deep_gov_value(struct ai_type *ait, struct player *pplayer,
                    struct government *gov, adv_want *val, bool *override)
{
  dai_gov_value(ait, pplayer, gov, val, override);
}
