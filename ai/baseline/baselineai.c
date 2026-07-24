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
#include "support.h"

/* common */
#include "ai.h"

/* ai/baseline/classic (vendored; fc_ai_classic_setup is renamed to
 * bl_fc_ai_classic_setup by the force-included baseline_syms.h) */
#include "classicai.h"

#include "baselineai.h"

/**********************************************************************//**
  Set up the vendored copy of the stock AI under the name "baseline".
**************************************************************************/
bool fc_ai_baseline_setup(struct ai_type *ai)
{
  if (!fc_ai_classic_setup(ai)) {
    return FALSE;
  }

  strncpy(ai->name, "baseline", sizeof(ai->name));

  return TRUE;
}
