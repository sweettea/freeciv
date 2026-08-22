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
#include "mem.h"
#include "shared.h"
#include "support.h"

/* common */
#include "city.h"
#include "extras.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "terrain.h"
#include "tile.h"
#include "world_object.h"

/* server */
#include "maphand.h"

/* server/advisors */
#include "advbuilding.h"
#include "advtools.h"

#include "deepdefence.h"
#include "deeptrade.h"
#include "deeptuning.h"

#include "deeptrunk.h"

/* Distance histogram for beyond-ceiling picks: one bucket per map
 * distance 0..DEEP_TRUNK_SRC_HIST-1, and a last bucket for everything
 * farther. Instrumentation shape, not a tunable: it changes what the
 * log line says, never what any unit does. */
#define DEEP_TRUNK_SRC_HIST 16

/* One planned link between two cities, measured over the tiles the
 * corridor walk actually visited. */
struct deep_trunk_link {
  int terr;                  /* Terrain movement cost over the corridor,
                              * in move fragments. */
  int len;                   /* Corridor tiles (city ends excluded). */
  int gap;                   /* Of those, still without a road that
                              * speeds movement. */
  bool ally;                 /* Far endpoint is an allied city, not one
                              * of ours (W12 slice 3). */
  double dividend;           /* Discounted caravan route value the
                              * finished link unlocks (W12 slice 4), in
                              * deeptrade's own units. */
};

/* How a corridor tile's owner stands to the player planning the
 * corridor. The whole point of the split is that these classes are NOT
 * interchangeable: what a worker may legally do differs between them
 * (nothing does, in the core rules -- see deep_trunk_own_class()), what
 * deep's own policy allows differs between them, and what it costs to
 * be caught standing there differs most of all.
 *
 * Ordering is the log line's field order and nothing else depends on
 * it. */
enum deep_trunk_own {
  DEEP_TRUNK_OWN_NONE = 0,     /* Unclaimed ground. */
  DEEP_TRUNK_OWN_SELF,         /* Ours. */
  DEEP_TRUNK_OWN_ALLY,         /* DS_ALLIANCE or DS_TEAM. */
  DEEP_TRUNK_OWN_PEACE,        /* DS_PEACE or DS_ARMISTICE. */
  DEEP_TRUNK_OWN_CEASE,        /* DS_CEASEFIRE. */
  DEEP_TRUNK_OWN_WAR,          /* DS_WAR. */
  DEEP_TRUNK_OWN_OTHER,        /* DS_NO_CONTACT and anything later. */
  DEEP_TRUNK_OWN_COUNT
};

/* The plan, memoised on (turn, player): one link index per tile
 * (0 = none, otherwise link+1) and the links themselves. */
static int *deep_trunk_tile = NULL;
static int deep_trunk_tiles = 0;
static struct deep_trunk_link *deep_trunk_links = NULL;
static int deep_trunk_nlinks = 0;
static int deep_trunk_alloc = 0;
static int deep_trunk_turn = -1;
static int deep_trunk_plr = -1;

/* The corridor gap tiles the advisor can NEVER offer: those outside
 * every own city radius (autoworkers.c:586). Tile indices, rebuilt
 * with the plan. */
static int *deep_trunk_beyond = NULL;
static int deep_trunk_nbeyond = 0;
static int deep_trunk_beyond_alloc = 0;

/* Per-player census, reset per turn by deep_trunk_census(). */
static int deep_trunk_c_links[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_gap[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_hits[MAX_NUM_PLAYER_SLOTS];
static double deep_trunk_c_max[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_beyond[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_src_off[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_src_pick[MAX_NUM_PLAYER_SLOTS];
static int
deep_trunk_c_src_hist[MAX_NUM_PLAYER_SLOTS][DEEP_TRUNK_SRC_HIST + 1];
/* W12 slice 3, the ally corridors' own instruments: links planned to
 * an allied city, their corridor tiles still missing a road, and how
 * many of those the task source offered and picked. */
static int deep_trunk_c_ally_links[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_ally_gap[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_ally_off[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_ally_pick[MAX_NUM_PLAYER_SLOTS];
/* W12 slice 4, the trade dividend's own instruments: route pairs
 * actually valued this turn (the CPU cost, visible), links that came
 * out carrying a dividend, and the largest one. */
static int deep_trunk_c_div_pairs[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_div_links[MAX_NUM_PLAYER_SLOTS];
static double deep_trunk_c_div_max[MAX_NUM_PLAYER_SLOTS];
/* Beyond-ceiling picks that landed on a dividend-priced corridor, and
 * of those the ones the dividend actually FLIPPED -- the source would
 * have assigned a different task (or none) without it. */
static int deep_trunk_c_div_pick[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_div_flip[MAX_NUM_PLAYER_SLOTS];
/* W12H: the soft-border ledger, split by the treaty state of the tile
 * OWNER (deep_trunk_own_class()). plan/work/road are properties of the
 * plan and are recomputed by every refresh; off/pick accumulate over
 * the player-turn like every other source counter. */
static int deep_trunk_c_soft_plan[MAX_NUM_PLAYER_SLOTS]
                                 [DEEP_TRUNK_OWN_COUNT];
static int deep_trunk_c_soft_off[MAX_NUM_PLAYER_SLOTS]
                                [DEEP_TRUNK_OWN_COUNT];
static int deep_trunk_c_soft_pick[MAX_NUM_PLAYER_SLOTS]
                                 [DEEP_TRUNK_OWN_COUNT];
static int deep_trunk_c_soft_work[MAX_NUM_PLAYER_SLOTS]
                                 [DEEP_TRUNK_OWN_COUNT];
static int deep_trunk_c_soft_road[MAX_NUM_PLAYER_SLOTS]
                                 [DEEP_TRUNK_OWN_COUNT];

/* Last refresh's plan tiles and their ownership classes, per player:
 * what `road=` is measured against. A tile that was in the plan (so
 * unroaded by construction) and carries a road at the next refresh has
 * BEEN ROADED -- by us or by its owner, which this cannot distinguish
 * and does not claim to. `work=` is the attributable half. */
struct deep_trunk_seen {
  int *tile;
  unsigned char *own;
  int n, alloc;
};

static struct deep_trunk_seen deep_trunk_prev[MAX_NUM_PLAYER_SLOTS];

/* W12H, the risk term's own instruments: corridor steps the walk
 * priced, the ones where the risk-priced choice differed from the
 * distance-first one today's walk takes (the reroutes, which is the
 * whole visible effect), the risk actually walked over and its worst
 * tile, and how many chosen tiles carried the coastal term. Plan
 * properties, so recomputed by every refresh. */
/* W12H, the exposure ledger the employment epic's standing question
 * wants: which workers stood on a corridor tile at the last refresh,
 * so this refresh can say which of them are gone. Attribution is by the
 * tile they were standing on -- whose ground it was and whether it was
 * ocean-adjacent -- which is the split the question is asked in.
 *
 * "Gone" is honest rather than precise: a unit id that no longer
 * resolves has died, been disbanded, been captured or joined a city.
 * On corridor duty the first is much the commonest, and the alternative
 * -- a death hook the vtable does not offer -- does not exist. */
struct deep_trunk_duty {
  int unit;
  int tile;
  unsigned char own;
  unsigned char coastal;
};

static struct deep_trunk_duty *deep_trunk_duty[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_duty_n[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_duty_alloc[MAX_NUM_PLAYER_SLOTS];

/* Cumulative over the game, so the LAST line per (game, player) is that
 * game's total -- the same shape deep-trunkfun uses. */
static int deep_trunk_c_duty_seen[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_duty_lost[MAX_NUM_PLAYER_SLOTS]
                                 [DEEP_TRUNK_OWN_COUNT];
static int deep_trunk_c_duty_coast[MAX_NUM_PLAYER_SLOTS];

static int deep_trunk_c_risk_steps[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_risk_dodge[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_risk_fear[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_risk_coast[MAX_NUM_PLAYER_SLOTS];
static double deep_trunk_c_risk_sum[MAX_NUM_PLAYER_SLOTS];
static double deep_trunk_c_risk_max[MAX_NUM_PLAYER_SLOTS];

/* W12G: corridor bids the founder's own site value refused, corridor
 * tiles a worker was already walking to and that the source priced
 * again this turn, and the subset of those the sunk-walk premium
 * actually held on to. */
static int deep_trunk_c_keep_veto[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_keep_held[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_keep_kept[MAX_NUM_PLAYER_SLOTS];

/* W28: occupied corridor tiles the source met, the ones it managed to
 * price as a join, the ones whose join saved no whole turn, the joins
 * actually assigned, and the whole turns those assignments were priced
 * to save. */
static int deep_trunk_c_stk_seen[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_stk_off[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_stk_dec[MAX_NUM_PLAYER_SLOTS][3];
static int deep_trunk_c_stk_pick[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_stk_save[MAX_NUM_PLAYER_SLOTS];

/* W12G: which corridor tile each worker is walking to.
 *
 * The advisor keeps no such memory -- auto_workers_player() sends an
 * IDLE unit to settler_run and adv_follow_path() restores IDLE after
 * every partial walk (advgoto.c:65-72), so a walking worker is
 * re-decided from scratch every turn and `punit->server.adv->task`
 * holds the KIND of task and not its tile. The stickiness margin needs
 * the tile, so this is it: one entry per worker holding a corridor
 * assignment, allocated only when the margin is open.
 *
 * Deliberately NOT the funnel's ledger, which looks identical and is
 * not: deepfunnel.c allocates nothing unless the metric lines are being
 * written (deepfunnel.c:106), so reading it here would make what the AI
 * DOES depend on whether it is being logged.
 *
 * The slot count is a bookkeeping bound like deepfunnel's: a worker
 * that cannot be recorded simply gets no premium. Entries for units
 * that never return to the source (dead, held, mid-activity elsewhere)
 * are evicted least-recently-touched-first when the table fills. */
#define DEEP_TRUNK_HOLD_SLOTS 512

struct deep_trunk_hold {
  int unit;                  /* Unit id; 0 marks a free slot. */
  int tile;                  /* Tile index of the corridor tile held. */
  int turn;                  /* Turn the hold was last written. */
};

static struct deep_trunk_hold
deep_trunk_holds[MAX_NUM_PLAYER_SLOTS][DEEP_TRUNK_HOLD_SLOTS];

/**********************************************************************//**
  Does this tile already carry a road that speeds movement? A corridor
  tile that does needs no further work for the link's sake -- the
  ruleset's later upgrades (rail) are a different question, priced by
  the advisor's own move-cost term on the tile.
**************************************************************************/
bool deep_trunk_tile_roaded(const struct tile *ptile)
{
  extra_type_by_cause_iterate(EC_ROAD, pextra) {
    if (tile_has_extra(ptile, pextra)
        && road_provides_move_bonus(extra_road_get(pextra))) {
      return TRUE;
    }
  } extra_type_by_cause_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  Which ownership class this tile falls in for 'pplayer'.

  Recorded here rather than assumed, because the corridor family has
  twice reasoned from "a worker may not build there" when the rules say
  no such thing. What the CORE RULES forbid a civilian, by state, is:
  nothing.

  * Entry. players_non_invade() (common/player.c:1434) is TRUE for
    DS_PEACE alone -- the in-code comment says armistice is deliberately
    left open so units can withdraw. Every caller of it guards on
    !utype_has_flag(utype, UTYF_CIVILIAN): the move test
    (common/movement.c:773-777, MR_PEACE), the teleport test
    (movement.c:894-898) and all three pathfinder node initialisers
    (common/aicore/path_finding.c:302-303, :1053-1054, :2092-2093).
    Workers and Engineers carry NonMil (data/classic/units.ruleset:474,
    :523), so a worker may walk into ANY player's borders in ANY state.
    Even the armistice-ripens purge spares them explicitly
    (remove_illegal_armistice_units(), server/srv_main.c:915-946, which
    skips is_enter_borders_unit()).
  * Building. can_build_road() (common/road.c:302-314) reaches
    can_build_extra_base() (common/extras.c:413-462, no tile_owner
    reference at all) and the ruleset reqs; classic's Build Road enabler
    asks for the Workers flag and CanRoad terrain and nothing else
    (data/classic/actions.ruleset:1065-1074). tile_owner() enters road
    building at exactly one place, as the `other` req context
    (road.c:276-281), which is inert unless a ruleset writes a DiplRel
    requirement -- and neither classic nor civ2civ3 does. The contrast
    is player_can_place_extra() (extras.c:487-529), the INFRAPOINTS
    path, which does demand tile_owner(ptile) == pplayer.
  * Consequences. No casus belli and no incident is attached to entry
    or to ACTRES_ROAD: action_consequence_*() is called for spy actions,
    capture/wipe/expel/conquer-extras/heal, paradrop and -- among worker
    activities -- ACTIVITY_PILLAGE alone (server/unittools.c:1035-1048).
    A finished road leaves the tile's owner unchanged; create_extra()
    re-claims BASES only (server/maphand.c:2549).

  So the ban deep and the advisor both enforce today -- TB_IGNORE on
  every non-allied owner (autoworkers.c:521-535,
  deep_settler_tile_behavior()) -- is ADVISOR POLICY, not law. That
  makes each class below a policy decision this file gets to make on its
  own merits, and the merit is risk, not legality: a worker on a war
  enemy's ground dies, a worker on a peace partner's ground does not.
**************************************************************************/
static enum deep_trunk_own
deep_trunk_own_class(const struct player *pplayer, const struct tile *ptile)
{
  const struct player *owner = tile_owner(ptile);

  if (owner == NULL) {
    return DEEP_TRUNK_OWN_NONE;
  }
  if (owner == pplayer) {
    return DEEP_TRUNK_OWN_SELF;
  }
  switch (player_diplstate_get(pplayer, owner)->type) {
  case DS_ALLIANCE:
  case DS_TEAM:
    return DEEP_TRUNK_OWN_ALLY;
  case DS_PEACE:
  case DS_ARMISTICE:
    /* One class, because a peace treaty lands as DS_ARMISTICE and
     * ripens into DS_PEACE by itself (server/diplhand.c:593 and
     * server/srv_main.c:970-984). Splitting them would make a corridor
     * appear and vanish on a countdown neither player chose. */
    return DEEP_TRUNK_OWN_PEACE;
  case DS_CEASEFIRE:
    return DEEP_TRUNK_OWN_CEASE;
  case DS_WAR:
    return DEEP_TRUNK_OWN_WAR;
  default:
    return DEEP_TRUNK_OWN_OTHER;
  }
}

/**********************************************************************//**
  W12H: may a corridor cross this tile, given who owns it?

  TRUE for ground the plan and the worker have always been able to use
  (unowned, ours, an ally's), and additionally -- behind
  trunk_soft_border_gate -- for a peace partner's. The single definition
  both legs share: the planner's border clip (deep_trunk_walk()) and the
  corridor pathfinder's tile behaviour (deep_corridor_tile_behavior(),
  deepsettler.c) must agree exactly, or the plan values tiles no worker
  can reach or the workers walk to tiles no plan wants.
**************************************************************************/
bool deep_trunk_soft_open(const struct player *pplayer,
                          const struct tile *ptile)
{
  switch (deep_trunk_own_class(pplayer, ptile)) {
  case DEEP_TRUNK_OWN_NONE:
  case DEEP_TRUNK_OWN_SELF:
  case DEEP_TRUNK_OWN_ALLY:
    return TRUE;
  case DEEP_TRUNK_OWN_PEACE:
    return deep_tuning_get()->trunk_soft_border_gate > 0;
  default:
    /* War, cease-fire, no contact: a worker out there dies, and the
     * gate is about safety rather than legality. */
    return FALSE;
  }
}

/**********************************************************************//**
  Remember this refresh's plan tiles and their ownership classes, so the
  next refresh can say which of them gained a road.
**************************************************************************/
static void deep_trunk_seen_add(int pn, int idx, enum deep_trunk_own own)
{
  struct deep_trunk_seen *seen = &deep_trunk_prev[pn];

  if (seen->n >= seen->alloc) {
    seen->alloc = MAX(2 * seen->alloc, 64);
    seen->tile = fc_realloc(seen->tile, seen->alloc * sizeof(*seen->tile));
    seen->own = fc_realloc(seen->own, seen->alloc * sizeof(*seen->own));
  }
  seen->tile[seen->n] = idx;
  seen->own[seen->n] = (unsigned char) own;
  seen->n++;
}

/**********************************************************************//**
  W12H: what a corridor tile is worth being afraid of, in deep's own
  standing-fear units.

  Two terms, both deep's existing ones, evaluated at a tile instead of
  at a city:

  * Land. deep_density_fear_at() -- every met non-allied city within
    density_fear_radius contributing density_fear_value / (1 + distance),
    overseas neighbours discounted by menace_overseas_weight until they
    float transports. Near a rival's cities is expensive; empty
    borderland between two empires is nearly free, which is the shape
    the brief asks for and the shape the term already had.
  * Coast. boat_shadow_fear on a tile adjacent to ocean, once
    deep_boat_shadow_afloat() says some player we are at war with owns
    a transport. Same no-fear-before-the-threat-class law the city term
    obeys: before anyone can sail, a beach is just a tile.

  Not a second risk model, by construction: every number here is read
  from deepdefence, and tuning the garrison's fear tunes the road.
**************************************************************************/
static double deep_trunk_tile_risk(const struct player *pplayer,
                                   const struct tile *ptile, bool afloat,
                                   bool *coastal_out)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double risk = deep_density_fear_at(pplayer, ptile);
  bool coastal = (afloat && tune->boat_shadow_fear > 0
                  && is_terrain_class_near_tile(&(wld.map), ptile,
                                                TC_OCEAN));

  if (coastal) {
    risk += tune->boat_shadow_fear;
  }
  if (coastal_out != NULL) {
    *coastal_out = coastal;
  }

  return risk;
}

/**********************************************************************//**
  The cost of stepping onto this tile: what the corridor already pays
  for it in movement (terrain movement_cost * SINGLE_MOVE, the very
  quantity link->terr sums) plus trunk_risk_weight times its risk.

  One currency, move fragments, because the corridor's whole value is
  denominated in them -- deep_trunk_price() pays trunk_road_gain per
  whole movement point saved. The weight is what says how many fragments
  a point of fear is worth, and nothing measures that yet.
**************************************************************************/
static double deep_trunk_step_cost(const struct player *pplayer,
                                   const struct tile *ptile, bool afloat,
                                   bool *coastal_out)
{
  const struct deep_tuning *tune = deep_tuning_get();

  return (double) (tile_terrain(ptile)->movement_cost * SINGLE_MOVE)
         + tune->trunk_risk_weight
           * deep_trunk_tile_risk(pplayer, ptile, afloat, coastal_out);
}

/**********************************************************************//**
  Walk a corridor from 'from' to 'to', greedily: each step goes to the
  land neighbour strictly nearer the destination, so the walk cannot
  loop and terminates in at most real_map_distance() steps. Returns the
  number of intermediate tiles written to 'path', or -1 when the walk
  runs out of land -- a link that has to cross water is not a road link
  and is dropped whole rather than half-marked.

  With 'clip' the walk also stops at the first tile
  deep_trunk_soft_open() refuses, returning the corridor up to that
  border. Building anywhere is legal -- the classic ruleset's road
  enabler asks only for the Workers flag and CanRoad terrain
  (actions.ruleset:1066), and ACTRES_ROAD carries no hard ownership
  requirement (common/actions.c) -- so what the clip encodes is where a
  worker will be SENT, not where it may go. That is why the two must
  agree: deep_corridor_tile_behavior() (deepsettler.c) opens exactly the
  same set to the corridor pathfinder, so a clipped tile is one no
  worker would path to and an unclipped one is want that can be spent.

  Greedy and not pathfinding on purpose. This runs once per city pair
  per turn and its output is a corridor to VALUE, not a route to
  follow; a worker's own pathfinding decides how it gets there.
**************************************************************************/
static int deep_trunk_walk(const struct civ_map *nmap,
                           const struct player *pplayer,
                           struct tile *from, struct tile *to,
                           struct tile **path, int maxlen, bool clip,
                           bool afloat)
{
  const struct deep_tuning *tune = deep_tuning_get();
  bool risky = (tune->trunk_risk_weight > 0.0);
  int pn = player_number(pplayer);
  struct tile *cur = from;
  int dist = real_map_distance(from, to);
  int n = 0;

  while (dist > 1) {
    struct tile *step = NULL, *plain = NULL, *flat = NULL;
    int stepdist = dist;
    double stepcost = 0.0, flatcost = 0.0;

    adjc_iterate(nmap, cur, atile) {
      int d;

      if (is_ocean_tile(atile)) {
        continue;
      }
      d = real_map_distance(atile, to);
      if (d < stepdist) {
        stepdist = d;
        step = atile;
        /* The tile today's walk takes: the FIRST neighbour reaching the
         * best distance, iteration order deciding. Kept so the census
         * can charge the risk term for the steps it actually moved. */
        plain = atile;
        /* And the tile the SAME comparison would take with the risk
         * term removed -- terrain alone. The counterfactual, so the
         * census can say the RISK moved this step rather than that the
         * edge cost did: folding terrain into the cost means the
         * smallest positive weight already reroutes for terrain, and
         * without this the two are inseparable. Same shape as slice 4's
         * dividend-free champion. */
        flat = atile;
        if (risky) {
          stepcost = deep_trunk_step_cost(pplayer, atile, afloat, NULL);
          flatcost = tile_terrain(atile)->movement_cost * SINGLE_MOVE;
        }
      } else if (risky && step != NULL && d == stepdist) {
        double c = deep_trunk_step_cost(pplayer, atile, afloat, NULL);
        double f = tile_terrain(atile)->movement_cost * SINGLE_MOVE;

        if (c < stepcost) {
          stepcost = c;
          step = atile;
        }
        if (f < flatcost) {
          flatcost = f;
          flat = atile;
        }
      }
    } adjc_iterate_end;

    if (step == NULL || n >= maxlen) {
      return -1;
    }
    if (risky) {
      bool coastal = FALSE;
      double risk = deep_trunk_tile_risk(pplayer, step, afloat, &coastal);

      deep_trunk_c_risk_steps[pn]++;
      deep_trunk_c_risk_sum[pn] += risk;
      deep_trunk_c_risk_max[pn] = MAX(deep_trunk_c_risk_max[pn], risk);
      if (coastal) {
        deep_trunk_c_risk_coast[pn]++;
      }
      if (step != plain) {
        deep_trunk_c_risk_dodge[pn]++;
      }
      if (step != flat) {
        /* The risk term ALONE moved this step: terrain would have
         * chosen differently, and only the fear separates them. */
        deep_trunk_c_risk_fear[pn]++;
      }
    }
    if (clip && !deep_trunk_soft_open(pplayer, step)) {
      return n;
    }
    path[n++] = step;
    cur = step;
    dist = stepdist;
  }

  return n;
}

/**********************************************************************//**
  W12 slice 3: append one trunk node per allied player -- that ally's
  city nearest (map distance) to any of our own cities on the same
  continent. Returns the number of nodes appended at 'ctile + nown'.

  One per ally and not the whole allied city list, because the MST is
  the point: n allied cities dropped into the node set would let Prim
  spend its edges inside the ally's empire, which the ally's own
  workers are already covering, and would dilute the one link that
  matters -- the shortest hop from our empire to theirs. Same-continent
  because a corridor is a road: deep_trunk_walk() drops any link it
  cannot walk over land anyway, so an overseas ally would cost a walk
  and yield nothing.
**************************************************************************/
static int deep_trunk_ally_nodes(const struct player *pplayer,
                                 struct tile **ctile, int nown)
{
  int added = 0;

  players_iterate_alive(aplayer) {
    struct tile *best = NULL;
    int bestd = FC_INFINITY;

    if (aplayer == pplayer || !pplayers_allied(pplayer, aplayer)) {
      continue;
    }

    city_list_iterate(aplayer->cities, acity) {
      struct tile *atile = city_tile(acity);
      int i;

      for (i = 0; i < nown; i++) {
        int d;

        if (tile_continent(ctile[i]) != tile_continent(atile)) {
          continue;
        }
        d = real_map_distance(ctile[i], atile);
        if (d < bestd) {
          bestd = d;
          best = atile;
        }
      }
    } city_list_iterate_end;

    if (best != NULL) {
      ctile[nown + added++] = best;
    }
  } players_iterate_alive_end;

  return added;
}

/* Defined with the pricing below; the plan needs it to know which road
 * the corridor would receive. */
static const struct road_type *
deep_trunk_road_target(const struct extra_type *target);

/**********************************************************************//**
  W12 slice 4: what this link is worth in TRADE, over and above the
  movement it saves -- the caravan routes the finished corridor makes
  worth walking.

  Deep already amortizes a route by travel time: deep_trade_pair_value()
  returns what a route is worth at the moment a caravan arrives, and
  caravan_discount (d) buries that value exponentially in the ETA. Route
  face value GROWS with distance while d^ETA shrinks with it, so the
  biggest routes on the map are exactly the ones slow travel suppresses.
  A road that cuts the walk from 30 turns to 10 multiplies the
  discounted value by d^-20. That is the dividend, and it is priced here
  as the brief states it:

    sum over served pairs of  value * (d^ETA_with - d^ETA_without)

  Three bounded approximations, each stated rather than hidden, because
  the exact answers would each cost a pathfind per pair per corridor and
  this runs once per link per player-turn:

  1. ETA is arithmetic, not pathfinding. The corridor's own walk
     measured its terrain cost (link->terr) over its own tiles
     (link->len), so link->terr / link->len is a MEASURED cost per tile
     on exactly this ground; the journey to a destination dist tiles
     away is priced at that rate, and the road subtracts the same
     'saving' the logistics term is made of. Both ETAs are then that
     cost over the caravan's own move rate. No constant is minted: every
     number comes from the corridor the plan just walked or from the
     ruleset.

  2. The served set is a distance test. A pair (near endpoint ->
     destination) is served by this corridor when the FAR endpoint is
     strictly closer to the destination than the near one is -- walking
     the corridor makes progress toward it. That includes the link's own
     two cities (the far endpoint is at distance 0 from itself), which
     is the pair the corridor most obviously serves.

  3. Route life is held at the full horizon rather than horizon - ETA,
     so the value factored out of the difference is the same on both
     sides of it. This overstates a distant route's tail; the ETA
     difference, which is the whole signal here, is unaffected.

  Reuse, deliberately total: the value is deeptrade's valuator, the
  discount is deeptrade's caravan_discount, and the reach filter is
  deeptrade's. Nothing about a trade route is decided in this file.
**************************************************************************/
static double deep_trunk_link_dividend(const struct player *pplayer,
                                       const struct deep_trunk_link *link,
                                       struct tile *from, struct tile *to,
                                       struct tile **path, int len)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double discount = tune->caravan_discount;
  const struct road_type *proad = NULL;
  /* unit_virtual_create() needs a mutable owner and this is the same
   * player; the ghost is never added to any list, so nothing about
   * pplayer changes. Looked up rather than cast away. */
  struct player *owner = player_by_number(player_number(pplayer));
  int pn = player_number(pplayer);
  double per_tile, saving, total = 0.0;
  struct tile *ends[2];
  int e, t;

  if (discount <= 0.0 || discount >= 1.0 || link->len <= 0) {
    /* d = 1 is no discount at all, and with no discount a shorter walk
     * is worth exactly nothing extra -- the dividend is identically
     * zero rather than approximately so. */
    return 0.0;
  }

  /* The road the corridor would actually receive: whatever the first
   * tile still missing one would be given. The pricing path re-derives
   * this per candidate extra; here one road stands for the link, which
   * is what makes the dividend a per-link number. */
  for (t = 0; t < len && proad == NULL; t++) {
    if (!deep_trunk_tile_roaded(path[t])) {
      proad = deep_trunk_road_target(next_extra_for_tile(path[t], EC_ROAD,
                                                         pplayer, NULL));
    }
  }
  if (proad == NULL) {
    return 0.0;
  }

  saving = (double) (link->terr - link->len * proad->move_cost);
  if (saving <= 0.0) {
    return 0.0;
  }
  per_tile = (double) link->terr / (double) link->len;

  ends[0] = from;
  ends[1] = to;

  for (e = 0; e < 2; e++) {
    struct tile *near_end = ends[e], *far_end = ends[1 - e];
    struct city *src = tile_city(near_end);
    struct deep_trade_ghost ghost;

    if (src == NULL || city_owner(src) != pplayer) {
      /* An allied endpoint's own routes are its own AI's business, and
       * its caravans are not ours to price. */
      continue;
    }
    if (!deep_trade_ghost_open(&ghost, owner, src) || ghost.move_rate <= 0) {
      continue;
    }

    players_iterate_alive(aplayer) {
      city_list_iterate(aplayer->cities, dest) {
        struct tile *dtile = city_tile(dest);
        double without, with, value;
        int dnear;

        if (dest == src
            || tile_continent(dtile) != tile_continent(near_end)
            || !map_is_known(dtile, pplayer)) {
          /* A caravan walks: another landmass is not served by a road,
           * and a city we have never seen is not a destination we could
           * have chosen. */
          continue;
        }
        dnear = real_map_distance(near_end, dtile);
        if (real_map_distance(far_end, dtile) >= dnear) {
          /* The corridor is not on the way there. */
          continue;
        }

        deep_trunk_c_div_pairs[pn]++;
        value = deep_trade_pair_value(pplayer, &ghost, src, dest);
        if (value <= 0.0) {
          continue;
        }

        without = dnear * per_tile;
        with = MAX(without - saving, 0.0);
        total += value * (pow(discount, with / ghost.move_rate)
                          - pow(discount, without / ghost.move_rate));
      } city_list_iterate_end;
    } players_iterate_alive_end;

    deep_trade_ghost_close(&ghost);
  }

  return total;
}

/**********************************************************************//**
  Span this player's cities with a minimum spanning tree on map
  distance and turn its edges into corridors.

  An MST and not every pair: the empire needs each city reachable from
  the rest, and n-1 links do that, while pricing all n(n-1)/2 pairs
  would pile the same want onto whatever tiles sit between the two
  densest clusters. Cities on different continents simply never share
  an edge -- Prim restarts on a new component instead, so an island
  empire gets one tree per island rather than one link across water.
**************************************************************************/
static void deep_trunk_build(const struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int pn = player_number(pplayer);
  int nown = city_list_size(pplayer->cities);
  int nnodes = nown;
  struct tile **ctile, **path;
  bool *seen, *is_ally;
  int *near_to, *near_d;
  char *dump;
  /* The boat shadow's precondition, asked ONCE per plan rather than
   * once per tile: it is a scan of every at-war player's unit list. */
  bool afloat = (tune->trunk_risk_weight > 0.0
                 && tune->boat_shadow_fear > 0
                 && deep_boat_shadow_afloat(pplayer));
  int i, j, maxpath, dumpsz;

  if (deep_trunk_tiles != MAP_INDEX_SIZE) {
    /* First call, or a new game with a differently sized map. */
    free(deep_trunk_tile);
    deep_trunk_tiles = MAP_INDEX_SIZE;
    deep_trunk_tile = fc_calloc(deep_trunk_tiles, sizeof(*deep_trunk_tile));
  } else {
    memset(deep_trunk_tile, 0,
           deep_trunk_tiles * sizeof(*deep_trunk_tile));
  }
  deep_trunk_nlinks = 0;
  deep_trunk_nbeyond = 0;

  /* Before the plan is rebuilt: which of last refresh's corridor tiles
   * have gained a road since. A plan tile is unroaded by construction,
   * so this is a completion count, not a stock. */
  memset(deep_trunk_c_soft_plan[pn], 0, sizeof(deep_trunk_c_soft_plan[pn]));
  memset(deep_trunk_c_soft_work[pn], 0, sizeof(deep_trunk_c_soft_work[pn]));
  memset(deep_trunk_c_soft_road[pn], 0, sizeof(deep_trunk_c_soft_road[pn]));
  deep_trunk_c_risk_steps[pn] = 0;
  deep_trunk_c_risk_dodge[pn] = 0;
  deep_trunk_c_risk_fear[pn] = 0;
  deep_trunk_c_risk_coast[pn] = 0;
  deep_trunk_c_risk_sum[pn] = 0.0;
  deep_trunk_c_risk_max[pn] = 0.0;
  for (i = 0; i < deep_trunk_prev[pn].n; i++) {
    if (deep_trunk_tile_roaded(index_to_tile(nmap,
                                             deep_trunk_prev[pn].tile[i]))) {
      deep_trunk_c_soft_road[pn][deep_trunk_prev[pn].own[i]]++;
    }
  }
  deep_trunk_prev[pn].n = 0;

  if (nown < 1) {
    return;
  }

  /* One node per own city, plus at most one per other player. */
  ctile = fc_malloc((nown + player_count()) * sizeof(*ctile));

  i = 0;
  city_list_iterate(pplayer->cities, pcity) {
    ctile[i++] = city_tile(pcity);
  } city_list_iterate_end;

  if (tune->trunk_ally_gate > 0) {
    nnodes += deep_trunk_ally_nodes(pplayer, ctile, nown);
  }

  if (nnodes < 2) {
    free(ctile);
    return;
  }

  if (deep_trunk_alloc < nnodes) {
    free(deep_trunk_links);
    deep_trunk_alloc = nnodes;
    deep_trunk_links = fc_malloc(deep_trunk_alloc
                                 * sizeof(*deep_trunk_links));
  }

  seen = fc_calloc(nnodes, sizeof(*seen));
  is_ally = fc_calloc(nnodes, sizeof(*is_ally));
  near_to = fc_malloc(nnodes * sizeof(*near_to));
  near_d = fc_malloc(nnodes * sizeof(*near_d));

  for (i = nown; i < nnodes; i++) {
    is_ally[i] = TRUE;
  }

  /* A greedy walk that must strictly close the gap every step cannot
   * take more steps than the map is wide plus tall. */
  maxpath = wld.map.xsize + wld.map.ysize;
  path = fc_malloc(maxpath * sizeof(*path));
  /* The tile list the dump below writes: "x,y;" per corridor tile,
   * with room for the widest coordinate pair this map can produce. */
  dumpsz = maxpath * 16 + 1;
  dump = (log_do_output_for_level(deep_metric_log_level())
          ? fc_malloc(dumpsz) : NULL);

  for (i = 0; i < nnodes; i++) {
    near_to[i] = -1;
    near_d[i] = FC_INFINITY;
  }
  seen[0] = TRUE;
  for (j = 1; j < nnodes; j++) {
    if (tile_continent(ctile[j]) == tile_continent(ctile[0])) {
      near_to[j] = 0;
      near_d[j] = real_map_distance(ctile[0], ctile[j]);
    }
  }

  for (i = 1; i < nnodes; i++) {
    int pick = -1, best = FC_INFINITY;

    for (j = 0; j < nnodes; j++) {
      if (!seen[j] && near_d[j] < best) {
        best = near_d[j];
        pick = j;
      }
    }
    if (pick < 0) {
      /* Every remaining city is on another landmass: start a fresh
       * component at the first one still unseen. */
      for (j = 0; j < nnodes; j++) {
        if (!seen[j]) {
          pick = j;
          break;
        }
      }
      if (pick < 0) {
        break;
      }
      near_to[pick] = -1;
    }

    seen[pick] = TRUE;

    if (near_to[pick] >= 0) {
      int len = deep_trunk_walk(nmap, pplayer, ctile[near_to[pick]],
                                ctile[pick], path, maxpath, is_ally[pick],
                                afloat);

      if (len > 0) {
        struct deep_trunk_link *link = &deep_trunk_links[deep_trunk_nlinks];
        int t;

        link->terr = 0;
        link->len = 0;
        link->gap = 0;
        link->ally = is_ally[pick];
        link->dividend = 0.0;
        for (t = 0; t < len; t++) {
          link->terr += tile_terrain(path[t])->movement_cost * SINGLE_MOVE;
          link->len++;
          if (!deep_trunk_tile_roaded(path[t])) {
            link->gap++;
          }
        }
        if (link->gap > 0) {
          /* A finished link is worth nothing more; only the tiles that
           * still need work carry the want, and only if some do. */
          for (t = 0; t < len; t++) {
            int idx = tile_index(path[t]);

            if (deep_trunk_tile[idx] == 0 && !deep_trunk_tile_roaded(path[t])) {
              enum deep_trunk_own own
                = deep_trunk_own_class(pplayer, path[t]);

              deep_trunk_tile[idx] = deep_trunk_nlinks + 1;
              deep_trunk_c_soft_plan[pn][own]++;
              deep_trunk_seen_add(pn, idx, own);
              if (deep_trunk_nbeyond >= deep_trunk_beyond_alloc) {
                deep_trunk_beyond_alloc = MAX(2 * deep_trunk_beyond_alloc,
                                              32);
                deep_trunk_beyond
                  = fc_realloc(deep_trunk_beyond,
                               deep_trunk_beyond_alloc
                               * sizeof(*deep_trunk_beyond));
              }
              deep_trunk_beyond[deep_trunk_nbeyond++] = idx;
            }
          }
          if (dump != NULL) {
            /* The plan's own geometry, written once per link per
             * player-turn: the tiles this corridor actually occupies,
             * in walk order, in MAP coordinates (TILE_XY). Two plans
             * of the same link -- one per gate setting, or one per
             * ally -- are only comparable if the tiles themselves are
             * on the record, which is what the reroute diff and the
             * meet-in-the-middle overlap both read. Instrumentation
             * shape: it changes what the log says, never what any
             * unit does. */
            int at = 0;

            for (t = 0; t < len; t++) {
              at += fc_snprintf(dump + at, dumpsz - at, "%s%d,%d",
                                t == 0 ? "" : ";", TILE_XY(path[t]));
            }
            log_base(deep_metric_log_level(),
                     "deep-trunklink turn=%d player=\"%s\" link=%d ally=%d"
                     " len=%d gap=%d from=%d,%d to=%d,%d tiles=%s",
                     game.info.turn, player_name(pplayer),
                     deep_trunk_nlinks, link->ally ? 1 : 0,
                     link->len, link->gap,
                     TILE_XY(ctile[near_to[pick]]), TILE_XY(ctile[pick]),
                     dump);
          }
          if (tune->trunk_trade_gain > 0.0) {
            /* Once per link per player-turn, inside the plan's own
             * (turn, player) memoisation -- which IS the cache the
             * brief asks for by (corridor, partner): the pricing path
             * reads the number, it never recomputes it. */
            link->dividend
              = deep_trunk_link_dividend(pplayer, link, ctile[near_to[pick]],
                                         ctile[pick], path, len);
          }
          deep_trunk_nlinks++;
        }
      }
    }

    if (is_ally[pick]) {
      /* An ally is an ENDPOINT, never a hub: relaxing from it could
       * make it the nearest neighbour of one of our own cities and put
       * a foreign capital in the middle of our own trunk. Skipping the
       * relaxation also fixes every ally link's direction as own ->
       * ally, which is what the border clip in deep_trunk_walk()
       * needs to mean "as far towards the ally as a worker can go". */
      continue;
    }

    for (j = 0; j < nnodes; j++) {
      if (!seen[j]
          && tile_continent(ctile[j]) == tile_continent(ctile[pick])) {
        int d = real_map_distance(ctile[pick], ctile[j]);

        if (d < near_d[j]) {
          near_d[j] = d;
          near_to[j] = pick;
        }
      }
    }
  }

  /* Of the gap tiles just collected, keep as "beyond" only those the
   * advisor can never enumerate: worker_evaluate_improvements() offers
   * exactly the tiles inside some own city's radius
   * (autoworkers.c:586), so subtract those. Marking is done by
   * negating the slot in deep_trunk_tile -- restored in the same pass
   * that compacts the list, so the array leaves this function
   * all-positive exactly as before. */
  if (deep_trunk_nbeyond > 0) {
    int m = 0;

    city_list_iterate(pplayer->cities, pcity) {
      city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                        city_tile(pcity), ptile) {
        int idx = tile_index(ptile);

        if (deep_trunk_tile[idx] > 0) {
          deep_trunk_tile[idx] = -deep_trunk_tile[idx];
        }
      } city_tile_iterate_end;
    } city_list_iterate_end;

    for (i = 0; i < deep_trunk_nbeyond; i++) {
      int idx = deep_trunk_beyond[i];

      if (deep_trunk_tile[idx] < 0) {
        deep_trunk_tile[idx] = -deep_trunk_tile[idx];
      } else {
        deep_trunk_beyond[m++] = idx;
      }
    }
    deep_trunk_nbeyond = m;
  }

  /* Own workers actually swinging a shovel on a corridor tile right
   * now, by whose ground it is. This is the ATTRIBUTABLE half of the
   * soft-border ledger -- the unit is ours, the activity is a road, and
   * the tile is on this turn's plan -- so it is what "deep built a road
   * on a peace partner's tile" should be read from. Sampled here rather
   * than in the census because the census runs after the plan's
   * single-slot memoisation may have moved on to another player. */
  /* Same pass answers the exposure question: first charge the previous
   * roster for the workers that are no longer there, then rebuild it
   * from who is standing on a corridor tile now. */
  for (i = 0; i < deep_trunk_duty_n[pn]; i++) {
    if (game_unit_by_number(deep_trunk_duty[pn][i].unit) == NULL) {
      deep_trunk_c_duty_lost[pn][deep_trunk_duty[pn][i].own]++;
      if (deep_trunk_duty[pn][i].coastal) {
        deep_trunk_c_duty_coast[pn]++;
      }
    }
  }
  deep_trunk_duty_n[pn] = 0;

  unit_list_iterate(pplayer->units, punit) {
    struct tile *utile = unit_tile(punit);
    enum deep_trunk_own own;

    if (deep_trunk_tile[tile_index(utile)] == 0
        || !unit_has_type_flag(punit, UTYF_WORKERS)) {
      continue;
    }
    own = deep_trunk_own_class(pplayer, utile);
    if (punit->activity == ACTIVITY_GEN_ROAD) {
      deep_trunk_c_soft_work[pn][own]++;
    }

    if (deep_trunk_duty_n[pn] >= deep_trunk_duty_alloc[pn]) {
      deep_trunk_duty_alloc[pn] = MAX(2 * deep_trunk_duty_alloc[pn], 32);
      deep_trunk_duty[pn]
        = fc_realloc(deep_trunk_duty[pn],
                     deep_trunk_duty_alloc[pn] * sizeof(*deep_trunk_duty[pn]));
    }
    deep_trunk_duty[pn][deep_trunk_duty_n[pn]].unit = punit->id;
    deep_trunk_duty[pn][deep_trunk_duty_n[pn]].tile = tile_index(utile);
    deep_trunk_duty[pn][deep_trunk_duty_n[pn]].own = (unsigned char) own;
    deep_trunk_duty[pn][deep_trunk_duty_n[pn]].coastal
      = is_terrain_class_near_tile(nmap, utile, TC_OCEAN) ? 1 : 0;
    deep_trunk_duty_n[pn]++;
    deep_trunk_c_duty_seen[pn]++;
  } unit_list_iterate_end;

  free(ctile);
  free(seen);
  free(is_ally);
  free(near_to);
  free(near_d);
  free(path);
  free(dump);
}

/**********************************************************************//**
  The trunk want for building 'target' on 'ptile'.

  The link is worth the movement it saves on one traversal once it is
  whole: the terrain cost of the corridor minus what the same corridor
  would cost under this road, in whole movement points. That is the
  connectivity benefit stated in a quantity the ruleset already
  defines, not a new scale.

  It is then discounted by amortize() over the tiles still missing --
  a link one tile short pays almost at once, a link twelve tiles short
  pays much later, and the same worker-turn is therefore spent closing
  the nearly-finished link first. The gap in tiles stands in for the
  gap in turns: the two differ by the worker rate, which is a
  multiplier common to every link and so cannot change their order.

  trunk_road_gain converts the saved movement into the advisor's tile
  currency and gates the whole term at 0.
**************************************************************************/
/* The road behind a candidate extra, if it is one that speeds
 * movement; NULL otherwise. The gate both pricing paths share. */
static const struct road_type *
deep_trunk_road_target(const struct extra_type *target)
{
  const struct road_type *proad;

  if (target == NULL || !is_extra_caused_by(target, EC_ROAD)) {
    return NULL;
  }
  proad = extra_road_get(target);
  if (proad == NULL || !road_provides_move_bonus(proad)) {
    return NULL;
  }

  return proad;
}

/**********************************************************************//**
  Build (or reuse) this player's plan for this turn. Memoised on
  (turn, player); also banks the per-turn census counts the build
  determines, so the census line can be emitted after the plan's
  scratch state has moved on to another player.
**************************************************************************/
static void deep_trunk_refresh(const struct player *pplayer)
{
  int pn = player_number(pplayer);
  int slot;

  if (deep_trunk_turn == game.info.turn && deep_trunk_plr == pn) {
    return;
  }
  deep_trunk_turn = game.info.turn;
  deep_trunk_plr = pn;
  deep_trunk_build(pplayer);
  deep_trunk_c_links[pn] = deep_trunk_nlinks;
  deep_trunk_c_gap[pn] = 0;
  deep_trunk_c_ally_links[pn] = 0;
  deep_trunk_c_ally_gap[pn] = 0;
  deep_trunk_c_div_links[pn] = 0;
  deep_trunk_c_div_max[pn] = 0.0;
  for (slot = 0; slot < deep_trunk_nlinks; slot++) {
    deep_trunk_c_gap[pn] += deep_trunk_links[slot].gap;
    if (deep_trunk_links[slot].ally) {
      deep_trunk_c_ally_links[pn]++;
      deep_trunk_c_ally_gap[pn] += deep_trunk_links[slot].gap;
    }
    if (deep_trunk_links[slot].dividend > 0.0) {
      deep_trunk_c_div_links[pn]++;
      deep_trunk_c_div_max[pn] = MAX(deep_trunk_c_div_max[pn],
                                     deep_trunk_links[slot].dividend);
    }
  }
  deep_trunk_c_beyond[pn] = deep_trunk_nbeyond;
}

/* Whether this tile's corridor runs to an allied city, on an
 * already-refreshed plan. The ally counters' only question. */
static bool deep_trunk_tile_ally(const struct tile *ptile)
{
  int slot;

  if (deep_trunk_tile == NULL) {
    return FALSE;
  }
  slot = deep_trunk_tile[tile_index(ptile)];

  return slot > 0 && deep_trunk_links[slot - 1].ally;
}

/* The want itself, on an already-refreshed plan: the movement the
 * corridor's link saves once whole, amortized over the tiles still
 * missing. Counter-free so both the hook path and the task source can
 * share it without polluting each other's instruments. */
static adv_want deep_trunk_price(const struct deep_tuning *tune,
                                 const struct extra_type *target,
                                 const struct tile *ptile,
                                 adv_want *div_out)
{
  const struct deep_trunk_link *link;
  const struct road_type *proad = deep_trunk_road_target(target);
  int saving, slot;

  if (div_out != NULL) {
    *div_out = 0.0;
  }

  if (proad == NULL) {
    return 0.0;
  }

  slot = deep_trunk_tile[tile_index(ptile)];
  if (slot == 0) {
    return 0.0;
  }
  link = &deep_trunk_links[slot - 1];

  saving = link->terr - link->len * proad->move_cost;
  if (saving <= 0) {
    return 0.0;
  }

  if (div_out != NULL) {
    *div_out = tune->trunk_trade_gain * link->dividend / MORT
               * TRADE_WEIGHTING * amortize(1.0, link->gap);
  }

  /* Logistics plus, behind trunk_trade_gain, the link's trade dividend
   * (W12 slice 4). The dividend arrives as a STOCK of discounted trade
   * points: / MORT turns it into the per-turn flow (amortize()'s own
   * annuity factor, the conversion deep_trade_merit_want() makes on the
   * same number), and * TRADE_WEIGHTING prices that flow in the
   * advisor's tile currency, which is what city_tile_value() pays for
   * every trade point a tile yields (infracache.c). Both factors are
   * the advisor's own; neither is minted here. Both terms carry the
   * same amortize() over the gap, because neither is paid until the
   * link is whole. */
  return (tune->trunk_road_gain * ((double) saving / SINGLE_MOVE)
          + tune->trunk_trade_gain * link->dividend / MORT * TRADE_WEIGHTING)
         * amortize(1.0, link->gap);
}

adv_want deep_trunk_want(const struct player *pplayer,
                         const struct extra_type *target,
                         const struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int pn = player_number(pplayer);
  adv_want want;

  if (tune->trunk_road_gain <= 0.0
      || deep_trunk_road_target(target) == NULL) {
    return 0.0;
  }

  deep_trunk_refresh(pplayer);

  want = deep_trunk_price(tune, target, ptile, NULL);
  if (want <= 0.0) {
    return 0.0;
  }

  deep_trunk_c_hits[pn]++;
  if (want > deep_trunk_c_max[pn]) {
    deep_trunk_c_max[pn] = want;
  }

  return want;
}

/**********************************************************************//**
  The corridor gap tiles outside every own city radius -- the demand
  the advisor's enumeration ceiling hides. Returns the count and
  points 'list' at the tile indices, valid until the next refresh.
  Builds (and memoises) this player's plan for this turn on first
  call, exactly as deep_trunk_want() does.
**************************************************************************/
int deep_trunk_beyond_tiles(const struct player *pplayer,
                            const int **list)
{
  deep_trunk_refresh(pplayer);
  *list = deep_trunk_beyond;

  return deep_trunk_nbeyond;
}

/**********************************************************************//**
  The trunk want for offering 'target' on 'ptile' as a task of deep's
  own sourcing, beyond the advisor's ceiling. The same pricing as
  deep_trunk_want() -- same plan, same saving, same amortize over the
  gap -- but counted on the source's own instrument rather than the
  hook's hits, so links>0 hits=0 keeps meaning what slice 1 said it
  means (a corridor the AUCTION never offered).
**************************************************************************/
adv_want deep_trunk_source_want(const struct player *pplayer,
                                const struct extra_type *target,
                                const struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  adv_want want;

  if (tune->trunk_road_gain <= 0.0) {
    return 0.0;
  }

  deep_trunk_refresh(pplayer);

  want = deep_trunk_price(tune, target, ptile, NULL);
  if (want > 0.0) {
    deep_trunk_c_src_off[player_number(pplayer)]++;
    deep_trunk_c_soft_off[player_number(pplayer)]
      [deep_trunk_own_class(pplayer, ptile)]++;
    if (deep_trunk_tile_ally(ptile)) {
      deep_trunk_c_ally_off[player_number(pplayer)]++;
    }
  }

  return want;
}

/**********************************************************************//**
  The TRADE DIVIDEND alone out of what deep_trunk_source_want() just
  said about this tile -- the part of the price W12 slice 4 added.

  It exists so the task source can price its own counterfactual: the
  champion it would have chosen with this term removed. That is the only
  honest way to say "the dividend flipped this pick" rather than "this
  pick happened to land on a corridor the dividend also priced", and it
  costs one subtraction because the number is already computed.
**************************************************************************/
adv_want deep_trunk_source_dividend(const struct player *pplayer,
                                    const struct extra_type *target,
                                    const struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  adv_want dividend = 0.0;

  if (tune->trunk_road_gain <= 0.0 || tune->trunk_trade_gain <= 0.0) {
    return 0.0;
  }

  deep_trunk_refresh(pplayer);
  deep_trunk_price(tune, target, ptile, &dividend);

  return dividend;
}

/**********************************************************************//**
  Whether this tile sits on a corridor to an allied city, for a caller
  that wants to say so in its own log line. Builds (and memoises) this
  player's plan for this turn on first call, like every other public
  entry point here.
**************************************************************************/
bool deep_trunk_tile_is_ally(const struct player *pplayer,
                             const struct tile *ptile)
{
  deep_trunk_refresh(pplayer);

  return deep_trunk_tile_ally(ptile);
}

/**********************************************************************//**
  Record that a beyond-ceiling task was actually assigned on 'ptile',
  'dist' map tiles from where the worker stood. Count and histogram
  feed the deep-trunksrc census line; a pick on an ally corridor also
  feeds the deep-trunkally line, which is the one instrument that says
  the gate did work rather than merely being open.
**************************************************************************/
void deep_trunk_source_pick(const struct player *pplayer,
                            const struct tile *ptile, int dist, bool flipped)
{
  int pn = player_number(pplayer);
  int bucket = CLIP(0, dist, DEEP_TRUNK_SRC_HIST);
  int slot = deep_trunk_tile == NULL ? 0 : deep_trunk_tile[tile_index(ptile)];

  deep_trunk_c_src_pick[pn]++;
  deep_trunk_c_src_hist[pn][bucket]++;
  deep_trunk_c_soft_pick[pn][deep_trunk_own_class(pplayer, ptile)]++;
  if (deep_trunk_tile_ally(ptile)) {
    deep_trunk_c_ally_pick[pn]++;
  }
  if (slot > 0 && deep_trunk_links[slot - 1].dividend > 0.0) {
    deep_trunk_c_div_pick[pn]++;
  }
  if (flipped) {
    deep_trunk_c_div_flip[pn]++;
  }
}

/**********************************************************************//**
  The corridor tile this worker is already walking to, or NULL when it
  holds none. Reading it costs one scan of this player's hold table.
**************************************************************************/
struct tile *deep_trunk_hold_tile(const struct player *pplayer, int uid)
{
  struct deep_trunk_hold *holds = deep_trunk_holds[player_number(pplayer)];
  int i;

  for (i = 0; i < DEEP_TRUNK_HOLD_SLOTS; i++) {
    if (holds[i].unit == uid) {
      return index_to_tile(&(wld.map), holds[i].tile);
    }
  }

  return NULL;
}

/**********************************************************************//**
  Remember that this worker is walking to 'ptile', or forget its hold
  when 'ptile' is NULL. Called once per worker per pass of the task
  source, so a worker that stops being offered a corridor stops being
  sticky on the same turn the source says so.
**************************************************************************/
void deep_trunk_hold_set(const struct player *pplayer, int uid,
                         const struct tile *ptile)
{
  struct deep_trunk_hold *holds = deep_trunk_holds[player_number(pplayer)];
  int i, free_slot = -1, oldest = 0;

  for (i = 0; i < DEEP_TRUNK_HOLD_SLOTS; i++) {
    if (holds[i].unit == uid) {
      if (ptile == NULL) {
        holds[i].unit = 0;
      } else {
        holds[i].tile = tile_index(ptile);
        holds[i].turn = game.info.turn;
      }
      return;
    }
    if (holds[i].unit == 0) {
      free_slot = (free_slot < 0 ? i : free_slot);
    } else if (holds[i].turn < holds[oldest].turn) {
      oldest = i;
    }
  }

  if (ptile == NULL) {
    return;
  }
  if (free_slot < 0) {
    /* Full: the least recently written hold belongs to the worker that
     * has gone longest without being offered a corridor, which is the
     * one whose walk is least likely to still be under way. */
    free_slot = oldest;
  }
  holds[free_slot].unit = uid;
  holds[free_slot].tile = tile_index(ptile);
  holds[free_slot].turn = game.info.turn;
}

/**********************************************************************//**
  Record that the source priced the tile a worker was already walking to
  ('kept' says the sunk-walk premium is why that worker is still walking
  to it -- without the premium the assignment would have changed).
**************************************************************************/
void deep_trunk_hold_census(const struct player *pplayer, bool kept)
{
  int pn = player_number(pplayer);

  deep_trunk_c_keep_held[pn]++;
  if (kept) {
    deep_trunk_c_keep_kept[pn]++;
  }
}

/**********************************************************************//**
  Record that the keepable-bid gate refused one corridor assignment:
  the source's best beyond-ceiling candidate outbid the advisor's own
  champion, so today's code would have assigned it, but it did not
  clear the floor the unit's own site search set and the assignment was
  never made. The count is the gate's whole visible effect and is zero
  whenever the gate is shut, because the floor is then zero and every
  candidate that beats the champion is picked.
**************************************************************************/
void deep_trunk_source_veto(const struct player *pplayer)
{
  deep_trunk_c_keep_veto[player_number(pplayer)]++;
}

/**********************************************************************//**
  W28's own instrument, one call per stage so the stages can be read
  against each other rather than only their ends.

  'seen' is an occupied corridor tile the source met at all -- the
  denominator, and the number today's skip throws away. 'offered' is one
  it went on to price as a join, the three declines are the three tests
  that can refuse one, and 'picked' a join assignment actually made,
  with 'saved' the whole turns it was priced to shave off that worksite.

  seen>0 with offered=0 is the gate shut, which is what the OFF arm must
  show; offered>0 with picked=0 is joins that were priced and outbid,
  which is a null rather than a dead knob.
**************************************************************************/
void deep_trunk_stack_note(const struct player *pplayer,
                           enum deep_trunk_stack_stage stage, int saved)
{
  int pn = player_number(pplayer);

  switch (stage) {
  case DEEP_TRUNK_STK_SEEN: deep_trunk_c_stk_seen[pn]++; break;
  case DEEP_TRUNK_STK_OFFER: deep_trunk_c_stk_off[pn]++; break;
  case DEEP_TRUNK_STK_DEC_IDLE: deep_trunk_c_stk_dec[pn][0]++; break;
  case DEEP_TRUNK_STK_DEC_LATE: deep_trunk_c_stk_dec[pn][1]++; break;
  case DEEP_TRUNK_STK_DEC_PART: deep_trunk_c_stk_dec[pn][2]++; break;
  case DEEP_TRUNK_STK_PICK:
    deep_trunk_c_stk_pick[pn]++;
    deep_trunk_c_stk_save[pn] += saved;
    break;
  }
}

/**********************************************************************//**
  One line per player per turn: the links the plan spans, the corridor
  tiles still without a road, how many candidates the want reached and
  the largest it issued. links>0 with hits=0 is the ceiling in
  deeptrunk.h speaking -- a planned corridor the advisor never offered
  because no city radius covers it.
**************************************************************************/
void deep_trunk_census(struct player *pplayer)
{
  int pn = player_number(pplayer);

  if (log_do_output_for_level(deep_metric_log_level())
      && deep_trunk_c_links[pn] > 0) {
    log_base(deep_metric_log_level(),
             "deep-trunk turn=%d player=\"%s\" links=%d gap=%d hits=%d"
             " maxwant=%.2f",
             game.info.turn, player_name(pplayer), deep_trunk_c_links[pn],
             deep_trunk_c_gap[pn], deep_trunk_c_hits[pn],
             deep_trunk_c_max[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && (deep_trunk_c_beyond[pn] > 0 || deep_trunk_c_src_off[pn] > 0
          || deep_trunk_c_src_pick[pn] > 0)) {
    /* One bucket per map distance plus the overflow, colon-separated;
     * beyond= is the demand the ceiling hides this turn, offered= the
     * candidates the source actually priced, picked= the tasks
     * assigned. beyond>0 with offered=0 is the source knob off (or no
     * worker free), the denominator the A/B reads in both arms. */
    char hist[(DEEP_TRUNK_SRC_HIST + 1) * 12];
    int b, at = 0;

    for (b = 0; b <= DEEP_TRUNK_SRC_HIST; b++) {
      at += fc_snprintf(hist + at, sizeof(hist) - at, "%s%d",
                        b == 0 ? "" : ":",
                        deep_trunk_c_src_hist[pn][b]);
    }
    log_base(deep_metric_log_level(),
             "deep-trunksrc turn=%d player=\"%s\" beyond=%d offered=%d"
             " picked=%d hist=%s",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_beyond[pn], deep_trunk_c_src_off[pn],
             deep_trunk_c_src_pick[pn], hist);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && (deep_trunk_c_ally_links[pn] > 0 || deep_trunk_c_ally_off[pn] > 0
          || deep_trunk_c_ally_pick[pn] > 0)) {
    /* W12 slice 3. links= corridors planned to an allied city this
     * turn, gap= their tiles still without a road (the demand the
     * gate exposed), offered= the ally corridor tiles the task source
     * priced, picked= the ally corridor tasks actually assigned.
     * links=0 in a game where deep HAS an ally is a miss, not a null:
     * the mechanism never ran. */
    log_base(deep_metric_log_level(),
             "deep-trunkally turn=%d player=\"%s\" links=%d gap=%d"
             " offered=%d picked=%d",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_ally_links[pn], deep_trunk_c_ally_gap[pn],
             deep_trunk_c_ally_off[pn], deep_trunk_c_ally_pick[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && (deep_trunk_c_div_pairs[pn] > 0
          || deep_trunk_c_div_pick[pn] > 0)) {
    /* W12 slice 4. pairs= route valuations this turn, which is the
     * mechanism's whole CPU cost made visible; links= corridors that
     * came out carrying a dividend; maxdiv= the largest, in deeptrade's
     * own units before the /MORT conversion. pairs>0 with links=0 says
     * the corridors serve no route worth walking, which is a null
     * rather than a broken knob. */
    log_base(deep_metric_log_level(),
             "deep-trunkdiv turn=%d player=\"%s\" pairs=%d links=%d"
             " maxdiv=%.2f picked=%d flipped=%d",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_div_pairs[pn], deep_trunk_c_div_links[pn],
             deep_trunk_c_div_max[pn], deep_trunk_c_div_pick[pn],
             deep_trunk_c_div_flip[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && (deep_trunk_c_keep_veto[pn] > 0 || deep_trunk_c_keep_held[pn] > 0)) {
    /* W12G. veto= corridor assignments the keepable-bid gate refused
     * this turn: bids that beat the advisor's champion and would have
     * been made, but did not clear the floor the unit's own site search
     * set. Each one is an assignment that today's code makes and then
     * discards inside the same call, so this counts the leak closed
     * rather than the mechanism running. held= workers the source
     * priced a corridor tile for that they were already walking to, and
     * kept= the subset where the sunk-walk premium is what kept them on
     * it -- the raw bid had lost. held>0 with kept=0 says the margin is
     * too small to change any auction it was offered, which is a null
     * rather than a dead knob. Clean zeros with both knobs shut are the
     * point, not a miss. */
    log_base(deep_metric_log_level(),
             "deep-trunkkeep turn=%d player=\"%s\" veto=%d held=%d kept=%d",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_keep_veto[pn], deep_trunk_c_keep_held[pn],
             deep_trunk_c_keep_kept[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && deep_trunk_c_stk_seen[pn] > 0) {
    /* W28. seen= occupied corridor tiles the source met this turn --
     * the ones it skips outright with the gate shut, so this line
     * exists in BOTH settings and its first field is the denominator
     * both arms share. offered= those priced as a join, declined= those
     * whose join saved no whole turn, picked= join assignments made and
     * saved= the whole turns they were priced to shave. seen>0 with
     * offered=0 IS the gate: nothing else zeroes the middle of this
     * line. declined= is the three refusals summed and dec= splits
     * them idle:late:part -- nobody was actually working the tile, the
     * job would end before this worker arrived, or the join bought no
     * whole turn. Only the last is the marginal test turning down a
     * real opportunity, so a null with dec= dominated by its first
     * field is a statement about the venue, not about the price. */
    log_base(deep_metric_log_level(),
             "deep-trunkstack turn=%d player=\"%s\" seen=%d offered=%d"
             " declined=%d dec=%d:%d:%d picked=%d saved=%d",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_stk_seen[pn], deep_trunk_c_stk_off[pn],
             deep_trunk_c_stk_dec[pn][0] + deep_trunk_c_stk_dec[pn][1]
             + deep_trunk_c_stk_dec[pn][2],
             deep_trunk_c_stk_dec[pn][0], deep_trunk_c_stk_dec[pn][1],
             deep_trunk_c_stk_dec[pn][2], deep_trunk_c_stk_pick[pn],
             deep_trunk_c_stk_save[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && deep_trunk_c_duty_seen[pn] > 0) {
    /* W12H exposure. on= workers standing on a corridor tile at this
     * turn's refresh; seen= worker-turns of corridor duty so far;
     * lost= the ones that stopped existing between two refreshes, by
     * the ownership class of the tile they were last seen on
     * (free:self:ally:peace:cease:war:other); coast= how many of those
     * losses were on ocean-adjacent ground. Cumulative except on=, so
     * the LAST line per game and player is that game's total. The
     * denominator seen= is the point: losses without exposure are a
     * rate nobody can read. */
    char lost[DEEP_TRUNK_OWN_COUNT * 12];
    int at = 0, c;

    for (c = 0; c < DEEP_TRUNK_OWN_COUNT; c++) {
      at += fc_snprintf(lost + at, sizeof(lost) - at, "%s%d",
                        c == 0 ? "" : ":", deep_trunk_c_duty_lost[pn][c]);
    }
    log_base(deep_metric_log_level(),
             "deep-trunkduty turn=%d player=\"%s\" on=%d seen=%d lost=%s"
             " coast=%d",
             game.info.turn, player_name(pplayer), deep_trunk_duty_n[pn],
             deep_trunk_c_duty_seen[pn], lost, deep_trunk_c_duty_coast[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && deep_trunk_c_risk_steps[pn] > 0) {
    /* W12H. steps= corridor steps the walk priced this turn, dodge= the
     * ones where the risk-priced choice differed from the distance-
     * first tile today's walk takes. fear= the subset of those the
     * RISK moved: the same comparison with the risk term removed
     * (terrain alone) would have chosen another tile, so fear= is
     * attributable to the fear and dodge= - fear= is the terrain half
     * the edge cost also introduces. risk= the standing fear actually
     * walked over, maxrisk= its worst tile, coast= chosen tiles
     * carrying the boat-shadow term. steps>0 with dodge=0 says the
     * corridors had no cheaper way round, which is a null rather than
     * a dead knob; the line does not exist at all at weight 0. */
    log_base(deep_metric_log_level(),
             "deep-trunkrisk turn=%d player=\"%s\" steps=%d dodge=%d"
             " fear=%d risk=%.2f maxrisk=%.2f coast=%d",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_risk_steps[pn], deep_trunk_c_risk_dodge[pn],
             deep_trunk_c_risk_fear[pn],
             deep_trunk_c_risk_sum[pn], deep_trunk_c_risk_max[pn],
             deep_trunk_c_risk_coast[pn]);
  }

  if (log_do_output_for_level(deep_metric_log_level())) {
    int foreign = 0, c;

    for (c = DEEP_TRUNK_OWN_ALLY; c < DEEP_TRUNK_OWN_COUNT; c++) {
      foreign += deep_trunk_c_soft_plan[pn][c] + deep_trunk_c_soft_off[pn][c]
                 + deep_trunk_c_soft_pick[pn][c]
                 + deep_trunk_c_soft_work[pn][c]
                 + deep_trunk_c_soft_road[pn][c];
    }
    if (foreign > 0) {
      /* W12H. Every field is the same seven-way ownership tuple, in
       * deep_trunk_own order:
       *
       *   free:self:ally:peace:cease:war:other
       *
       * plan= corridor tiles this turn's plan holds, off= the ones the
       * task source priced, pick= the assignments made on them, work=
       * own workers building a road on one right now, road= plan tiles
       * from LAST refresh that carry a road now (by anyone: the owner
       * builds roads too, which is why work= and not road= is the
       * attributable count). Printed only when some FOREIGN tile is
       * involved, so an empire whose corridors stay at home says
       * nothing at all. */
      char f[5][DEEP_TRUNK_OWN_COUNT * 12];
      const int *src[5];
      int k;

      src[0] = deep_trunk_c_soft_plan[pn];
      src[1] = deep_trunk_c_soft_off[pn];
      src[2] = deep_trunk_c_soft_pick[pn];
      src[3] = deep_trunk_c_soft_work[pn];
      src[4] = deep_trunk_c_soft_road[pn];
      for (k = 0; k < 5; k++) {
        int at = 0;

        for (c = 0; c < DEEP_TRUNK_OWN_COUNT; c++) {
          at += fc_snprintf(f[k] + at, sizeof(f[k]) - at, "%s%d",
                            c == 0 ? "" : ":", src[k][c]);
        }
      }
      /* peaceoff=/peacepick= repeat the PEACE column of off= and pick=
       * as scalars. Redundant on purpose: they are what the knob
       * census reads for trunk_soft_border_gate, and its predicates are
       * key=number matches that cannot index into a colon tuple. They
       * are also exactly the gate -- with it shut the corridor
       * pathfinder TB_IGNOREs a peace partner's tile, so it is never
       * pathed, never priced and never assigned, and both fields are 0
       * by construction however many such tiles the plan holds. */
      log_base(deep_metric_log_level(),
               "deep-trunksoft turn=%d player=\"%s\" plan=%s off=%s"
               " pick=%s work=%s road=%s peaceoff=%d peacepick=%d",
               game.info.turn, player_name(pplayer),
               f[0], f[1], f[2], f[3], f[4],
               deep_trunk_c_soft_off[pn][DEEP_TRUNK_OWN_PEACE],
               deep_trunk_c_soft_pick[pn][DEEP_TRUNK_OWN_PEACE]);
    }
  }

  memset(deep_trunk_c_soft_off[pn], 0, sizeof(deep_trunk_c_soft_off[pn]));
  memset(deep_trunk_c_soft_pick[pn], 0, sizeof(deep_trunk_c_soft_pick[pn]));
  deep_trunk_c_keep_veto[pn] = 0;
  deep_trunk_c_keep_held[pn] = 0;
  deep_trunk_c_keep_kept[pn] = 0;
  deep_trunk_c_stk_seen[pn] = 0;
  deep_trunk_c_stk_off[pn] = 0;
  memset(deep_trunk_c_stk_dec[pn], 0, sizeof(deep_trunk_c_stk_dec[pn]));
  deep_trunk_c_stk_pick[pn] = 0;
  deep_trunk_c_stk_save[pn] = 0;
  deep_trunk_c_div_pairs[pn] = 0;
  deep_trunk_c_div_links[pn] = 0;
  deep_trunk_c_div_max[pn] = 0.0;
  deep_trunk_c_div_pick[pn] = 0;
  deep_trunk_c_div_flip[pn] = 0;
  deep_trunk_c_links[pn] = 0;
  deep_trunk_c_gap[pn] = 0;
  deep_trunk_c_ally_links[pn] = 0;
  deep_trunk_c_ally_gap[pn] = 0;
  deep_trunk_c_ally_off[pn] = 0;
  deep_trunk_c_ally_pick[pn] = 0;
  deep_trunk_c_hits[pn] = 0;
  deep_trunk_c_max[pn] = 0.0;
  deep_trunk_c_beyond[pn] = 0;
  deep_trunk_c_src_off[pn] = 0;
  deep_trunk_c_src_pick[pn] = 0;
  memset(deep_trunk_c_src_hist[pn], 0,
         sizeof(deep_trunk_c_src_hist[pn]));
}
