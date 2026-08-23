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

/* common */
#include "terrain.h"

#include "advtools.h"

/**********************************************************************//**
  Amortize means gradually paying off a cost or debt over time. In freeciv
  terms this means we calculate how much less worth something is to us
  depending on how long it will take to complete.

  This is based on a global interest rate as defined by the MORT value.
**************************************************************************/
adv_want amortize(adv_want benefit, int delay)
{
  double discount = 1.0 - 1.0 / ((double)MORT);

  /* Note there's no rounding here.  We could round but it would probably
   * be better just to return (and take) a double for the benefit. */
  return benefit * pow(discount, delay);
}

/* The registered terrain-change valuation consumer, or NULL. One per
 * server: the consumer is a MODULE (ai/deep registers it at setup), and
 * which player is asking is answerable from the city the callback is
 * handed, so a per-player registry would carry no more information. */
static adv_terrain_value_fn adv_terrain_value_consumer = NULL;

/**********************************************************************//**
  Register (or, with NULL, withdraw) the terrain-change valuation
  consumer. See advtools.h: with none registered nothing changes.
**************************************************************************/
void adv_terrain_value_hook_set(adv_terrain_value_fn fn)
{
  adv_terrain_value_consumer = fn;
}

/**********************************************************************//**
  Ask the registered consumer what 'act' is worth on 'ptile' for 'pcity',
  given the one-step value the caller computed. Returns 'one_step'
  unchanged when nobody is registered.
**************************************************************************/
adv_want adv_terrain_value(const struct city *pcity,
                           const struct tile *ptile,
                           enum unit_activity act,
                           const struct terrain *result,
                           adv_want one_step)
{
  if (adv_terrain_value_consumer == NULL) {
    return one_step;
  }

  return adv_terrain_value_consumer(pcity, ptile, act, result, one_step);
}

/**********************************************************************//**
  Walk the transform chain from 'from': fill chain[] with successive
  terrains and total_time[] with cumulative worker-turn points to reach
  each. Stops at T_NONE, a repeat (Lake<->Grassland cycles), the start
  terrain, or ADV_TRANSFORM_CHAIN_MAX. Returns the number of steps.
**************************************************************************/
int adv_transform_chain(const struct terrain *from,
                        struct terrain *chain[ADV_TRANSFORM_CHAIN_MAX],
                        int total_time[ADV_TRANSFORM_CHAIN_MAX])
{
  const struct terrain *seen[ADV_TRANSFORM_CHAIN_MAX + 1];
  const struct terrain *cur = from;
  int n = 0, time = 0, i;

  seen[0] = from;

  while (n < ADV_TRANSFORM_CHAIN_MAX) {
    struct terrain *next = cur->transform_result;
    bool repeat = FALSE;

    if (next == T_NONE || next == cur) {
      break;
    }
    for (i = 0; i <= n; i++) {
      if (seen[i] == next) {
        repeat = TRUE;
        break;
      }
    }
    if (repeat) {
      break;
    }

    time += cur->transform_time;
    chain[n] = next;
    total_time[n] = time;
    n++;
    seen[n] = next;
    cur = next;
  }

  return n;
}

/**********************************************************************//**
  Total worker-turn points to transform 'from' into 'to' along the
  chain, or -1 if unreachable.
**************************************************************************/
int adv_transform_cost(const struct terrain *from,
                       const struct terrain *to)
{
  struct terrain *chain[ADV_TRANSFORM_CHAIN_MAX];
  int total_time[ADV_TRANSFORM_CHAIN_MAX];
  int n = adv_transform_chain(from, chain, total_time);
  int i;

  for (i = 0; i < n; i++) {
    if (chain[i] == to) {
      return total_time[i];
    }
  }
  return -1;
}
