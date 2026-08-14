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

/* common */
#include "city.h"
#include "combat.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "path_finding.h"
#include "pf_tools.h"

/* server/advisors */
#include "advgoto.h"

#include "deepai.h"
#include "deepdefence.h"
#include "deepnativity.h"
#include "deepodds.h"
#include "deepops.h"
#include "deeptuning.h"

#include "deepcombat.h"

/* Computational bounds, not tuning: the strongest units past either
 * cap carry essentially all of the probability mass. */
#define DEEP_WAVE_MAX 32
#define DEEP_STACK_MAX 16

struct deep_combatant {
  struct unit *punit;
  int rating;
  /* P(this combatant takes part at all). 1.0 for everything except an
   * attacker the garrison-pin discount weighs down (NETTHREAT item 2);
   * carried on the combatant so cap displacement in
   * deep_combatant_add() cannot separate a unit from its weight. Only
   * ever read for attackers (deep_garrison_vs_stream_full). */
  double weight;
};

/**********************************************************************//**
  Strongest first; unit id breaks ties so runs are deterministic.
**************************************************************************/
static int deep_combatant_cmp(const void *a_, const void *b_)
{
  const struct deep_combatant *a = a_, *b = b_;

  if (a->rating != b->rating) {
    return b->rating - a->rating;
  }
  return a->punit->id - b->punit->id;
}

/**********************************************************************//**
  Keep the strongest `cap` combatants seen so far.
**************************************************************************/
static void deep_combatant_add(struct deep_combatant *arr, int *n, int cap,
                               struct unit *punit, int rating,
                               double weight)
{
  int i, weakest = 0;

  if (*n < cap) {
    arr[*n].punit = punit;
    arr[*n].rating = rating;
    arr[*n].weight = weight;
    (*n)++;
    return;
  }
  for (i = 1; i < cap; i++) {
    if (arr[i].rating < arr[weakest].rating) {
      weakest = i;
    }
  }
  if (rating > arr[weakest].rating) {
    arr[weakest].punit = punit;
    arr[weakest].rating = rating;
    arr[weakest].weight = weight;
  }
}

/**********************************************************************//**
  The attacker's effective strength against this defender if it
  arrived FRESH. Enemy units are assessed at our phase start, when
  most have spent their moves; pricing them tired would let
  game.info.tired_attack scale a real threat toward zero. moves_left
  is restored before anything else can observe it.
**************************************************************************/
static int deep_fresh_attack_power(struct unit *att, const struct unit *def)
{
  int save = att->moves_left;
  int power;

  if (save < SINGLE_MOVE) {
    att->moves_left = SINGLE_MOVE;
  }
  power = get_total_attack_power(att, def, NULL);
  att->moves_left = save;

  return power;
}

/**********************************************************************//**
  The city's military garrison as a sorted (best first) defender list.
  `except` (may be NULL) is left out -- the sortie passes price a
  city's night without one of its blades this way.
**************************************************************************/
static int deep_collect_defenders(struct city *pcity,
                                  struct deep_combatant *def,
                                  const struct unit *except)
{
  struct player *pplayer = city_owner(pcity);
  int n_def = 0;

  unit_list_iterate(city_tile(pcity)->units, punit) {
    if (punit != except
        && unit_owner(punit) == pplayer
        && !unit_has_type_flag(punit, UTYF_CIVILIAN)
        && punit->hp > 0) {
      deep_combatant_add(def, &n_def, DEEP_STACK_MAX, punit,
                         adv_unit_def_rating_basic(punit), 1.0);
    }
  } unit_list_iterate_end;
  qsort(def, n_def, sizeof(def[0]), deep_combatant_cmp);

  return n_def;
}

/**********************************************************************//**
  P(the garrison outlives the stream): fill the per-pair parameters
  and run the chained-duel model. Shared by both reads -- the city's
  survival against an enemy wave, and (as 1 minus) our assault's
  chance against an enemy garrison.
**************************************************************************/
static double deep_garrison_vs_stream_full(const struct civ_map *nmap,
                                           struct deep_combatant *att,
                                           int n_att,
                                           struct deep_combatant *def,
                                           int n_def,
                                           double *att_death,
                                           double *def_death)
{
  int att_hp[DEEP_WAVE_MAX], def_hp[DEEP_STACK_MAX];
  double att_w[DEEP_WAVE_MAX];
  bool weighted = FALSE;
  struct deep_duel pair[DEEP_WAVE_MAX * DEEP_STACK_MAX];
  int i, j;

  if (att_death != NULL) {
    for (j = 0; j < n_att; j++) {
      att_death[j] = 0.0;
    }
  }
  if (def_death != NULL) {
    *def_death = 0.0;
  }
  if (n_att == 0) {
    return 1.0;
  }
  if (n_def == 0) {
    return 0.0;
  }

  for (j = 0; j < n_att; j++) {
    att_hp[j] = att[j].punit->hp;
    att_w[j] = att[j].weight;
    if (att[j].weight < 1.0) {
      weighted = TRUE;
    }
  }
  for (i = 0; i < n_def; i++) {
    def_hp[i] = def[i].punit->hp;
  }
  for (j = 0; j < n_att; j++) {
    for (i = 0; i < n_def; i++) {
      struct deep_duel *d = &pair[j * n_def + i];

      d->att_str = deep_fresh_attack_power(att[j].punit, def[i].punit);
      d->def_str = get_total_defense_power(att[j].punit, def[i].punit);
      get_modified_firepower(nmap, att[j].punit, def[i].punit,
                             &d->att_fp, &d->def_fp);
    }
  }

  /* NULL when nothing is weighted, so the unweighted cell -- and every
   * pre-knob caller -- takes the walk on the exact code path it always
   * did (byte-identity at threat_pin_discount = 0). */
  return deep_stack_survival_wfull(n_att, att_hp,
                                   weighted ? att_w : NULL,
                                   n_def, def_hp, pair,
                                   att_death, def_death);
}

/**********************************************************************//**
  The attacker-losses-only form, which is what every existing caller
  asks for.
**************************************************************************/
static double deep_garrison_vs_stream(const struct civ_map *nmap,
                                      struct deep_combatant *att, int n_att,
                                      struct deep_combatant *def, int n_def,
                                      double *att_death)
{
  return deep_garrison_vs_stream_full(nmap, att, n_att, def, n_def,
                                      att_death, NULL);
}

/**********************************************************************//**
  Probability that this city's garrison survives the wave of at-war
  units that could plausibly reach it within `horizon` turns, resolved
  as sequential duels to the death (deepodds.c). Strongest attackers
  strike first; the garrison presents its best defender until it
  falls.

  Real units only -- no unit is imagined here, and the anticipatory
  fear terms keep covering what this pass will not price. But NOT
  observed: the wave is assembled straight from aplayer->units with no
  visibility test of any kind, so it sees fogged and never-scouted
  stacks alike (RC6). The gates that do exist are at-war, contact-made
  (barbarians excepted, they never make contact), continent or coastal
  reachability, and arrival inside `horizon`. Making this honest is
  W04's job; until then every survival probability in this module is
  computed on ground truth.

  wave_out (optional) reports how many attackers qualified; with an
  empty wave the answer is 1.0 and with an empty garrison against any
  wave it is 0.0.
**************************************************************************/
double deep_city_stack_survival(const struct civ_map *nmap,
                                struct city *pcity, int horizon,
                                int *wave_out)
{
  return deep_city_stack_survival_without(nmap, pcity, horizon, NULL,
                                          wave_out);
}

/* One remembered verdict per city per phase. */
struct deep_survival_memo {
  int city_id;
  int horizon;
  double surv;
  int wave;
};

static struct deep_survival_memo *deep_survival_cache = NULL;
static int deep_survival_n = 0;
static int deep_survival_cap = 0;

/* HONEST LAND REACH (ERA 3c): one pathfinder map per attacker per
 * phase, shared by every city that asks about that attacker.
 *
 * The 3c instrument below measured the straight-line admission test
 * against the pathfinder on tech50war-200 (69 336 wave evaluations):
 * the furthest admitted attacker is unreachable outright in 19.1% of
 * evaluations and slower than assumed in another 25.6%, and the
 * pathfinder NEVER disagrees in the other direction (0.0% faster).
 * Confirming with the pathfinder therefore only ever REMOVES
 * attackers, which is why the straight-line test can stay in front as
 * a free prefilter: nothing it rejects would have been admitted.
 *
 * Cost containment, because deep_city_survival_core() is the hottest
 * routine deep has (437 440 calls over 32 probe games) and a
 * pf_map_new() per qualifying attacker per evaluation is not
 * affordable there: pf maps are lazily expanded, so ONE map per
 * attacker -- kept for the phase, keyed on the unit and the tile it
 * stood on when the map was drawn -- prices every (attacker, city)
 * question with a single shared Dijkstra frontier. Counterfactual
 * re-evaluations (_without/_with vary only defenders) hit the memo
 * every time. A map whose owner has moved since it was drawn is
 * redrawn: a stale origin is the same dishonesty this test exists to
 * remove. */
struct deep_reach_memo {
  int unit_id;
  int tile_index;           /* origin tile when the map was drawn */
  struct pf_map *pfm;
};

static struct deep_reach_memo *deep_reach_cache = NULL;
static int deep_reach_n = 0;
static int deep_reach_cap = 0;

/**********************************************************************//**
  Forget every remembered verdict. Called at the start of each player
  phase, which is what makes "once per city per turn" true and what
  keeps a verdict from a previous game in the same server process from
  ever being read.

  The reach memo shares the lifecycle for the same reason: the phase
  is the unit of coherence, and a pathfinder map may never outlive the
  phase whose unit positions it was drawn from.
**************************************************************************/
void deep_survival_cache_clear(void)
{
  int i;

  deep_survival_n = 0;

  for (i = 0; i < deep_reach_n; i++) {
    pf_map_destroy(deep_reach_cache[i].pfm);
  }
  deep_reach_n = 0;
}

/**********************************************************************//**
  The pathfinder's turn count for this attacker reaching this city, on
  the same attack parameter find_something_to_kill() uses
  (daiunit.c:603) -- terrain, ZOC and all. -1 means unreachable.
  Memoised per attacker per phase; see the reach memo above.
**************************************************************************/
static int deep_reach_pf_turns(const struct civ_map *nmap,
                               const struct unit *aunit,
                               struct tile *ctile)
{
  struct deep_reach_memo *m = NULL;
  struct pf_position pos;
  int i;

  for (i = 0; i < deep_reach_n; i++) {
    if (deep_reach_cache[i].unit_id == aunit->id) {
      m = &deep_reach_cache[i];
      break;
    }
  }
  if (m != NULL && m->tile_index != tile_index(unit_tile(aunit))) {
    /* The attacker moved since the map was drawn (phases are
     * concurrent); redraw from where it stands now. */
    pf_map_destroy(m->pfm);
    m->pfm = NULL;
  }
  if (m == NULL) {
    if (deep_reach_n >= deep_reach_cap) {
      deep_reach_cap = MAX(64, deep_reach_cap * 2);
      deep_reach_cache = fc_realloc(deep_reach_cache,
                                    deep_reach_cap
                                    * sizeof(*deep_reach_cache));
    }
    m = &deep_reach_cache[deep_reach_n++];
    m->unit_id = aunit->id;
    m->pfm = NULL;
  }
  if (m->pfm == NULL) {
    struct pf_parameter parameter;

    pft_fill_unit_attack_param(&parameter, nmap, aunit);
    m->tile_index = tile_index(unit_tile(aunit));
    m->pfm = pf_map_new(&parameter);
  }
  if (pf_map_position(m->pfm, ctile, &pos)) {
    return pos.turn;
  }
  return -1;
}

/**********************************************************************//**
  The city's survival verdict for THIS phase -- computed on the first
  question and remembered for the rest of them.

  "Will this city hold" is asked by the citizen arrangement at the top
  of the turn (deepcm.c, threshold cm_war_survival) and by the growth
  ladder at the bottom (deephorizon.c, threshold survival_target), and
  the two used to run the model independently against a map that had
  moved in between. Two thresholds are a defensible disagreement about
  what the answer is worth -- being wrong about a citizen costs food,
  being wrong about a garrison costs the city -- but two computations
  are a coin flip: the same city could be arranged for a siege and
  credited for growth in the same turn, which is a variance source in
  every ECON tuning cell and not a decision anyone made.

  Same call, same probability, thresholds intact and separate. The
  price is that the ladder reads a verdict taken before this turn's
  movement; the phase is the unit of coherence, deliberately.
**************************************************************************/
double deep_city_survival_verdict(const struct civ_map *nmap,
                                  struct city *pcity, int horizon,
                                  int *wave_out)
{
  int i;
  struct deep_survival_memo *e;

  for (i = 0; i < deep_survival_n; i++) {
    e = &deep_survival_cache[i];
    if (e->city_id == pcity->id && e->horizon == horizon) {
      if (wave_out != NULL) {
        *wave_out = e->wave;
      }
      return e->surv;
    }
  }

  if (deep_survival_n >= deep_survival_cap) {
    deep_survival_cap = MAX(64, deep_survival_cap * 2);
    deep_survival_cache = fc_realloc(deep_survival_cache,
                                     deep_survival_cap
                                     * sizeof(*deep_survival_cache));
  }
  e = &deep_survival_cache[deep_survival_n++];
  e->city_id = pcity->id;
  e->horizon = horizon;
  e->surv = deep_city_stack_survival(nmap, pcity, horizon, &(e->wave));
  if (wave_out != NULL) {
    *wave_out = e->wave;
  }

  return e->surv;
}

/* Observation only (ERA): how the wave that decided a survival
 * verdict was assembled. Counted while the reachability test runs, so
 * every field is a by-product of the qualification the model already
 * performed.
 *
 * The routes are the four disjunct arms of that test, in the order it
 * evaluates them, and they count QUALIFIED attackers -- which exceeds
 * n_att once the DEEP_WAVE_MAX cap starts displacing the weakest.
 * `capped` says when that happened, which today is silent. */
struct deep_wave_census {
  int by_land;
  int by_sea;
  int by_air;
  int by_transport;
  int n_qual;
  int max_dist;
  int max_turns;
  /* The furthest qualifying attacker itself, and ITS OWN
   * straight-line verdict -- max_turns is the max over the whole
   * wave and generally belongs to a different, slower unit, so the
   * two are not a comparable pair. far_* is the pair. */
  const struct unit *far_unit;
  int far_dist;
  int far_turns;
  /* NETTHREAT item 1's instrument: qualifying attackers that are
   * standing in a city one of OUR operations points at -- the wave
   * members our own offense is about to engage, and therefore the
   * mass any "offense consumes the wave" discount could ever net.
   * `net_pinned` counts every live operation, `net_marching` only the
   * ones that reached MARCH or STRIKE. Nothing acts on either. */
  int net_pinned;
  int net_marching;
  /* THE SEA ARM'S OWN REACH (2026-08-12). Until the move-type fix the
   * sea arm admitted nothing at all, so the standing worry that it has
   * no distance bound beyond horizon x move_rate was deferred as
   * "deferred, not dead". It is live now, and on the venue it matters
   * on it is not a bound: a destroyer moves 6 against a horizon of 3,
   * which admits 18 tiles, and a 32x32 torus is 16 tiles across.
   *
   * A tighter bound would be a new constant, which rule 7 forbids
   * without a measurement -- so MEASURE it. `sea_max_dist` is the
   * furthest sea-arm admission this evaluation saw; `sea_far` counts
   * the ones further away in TILES than the horizon is in TURNS, i.e.
   * the population admitted only because the unit moves more than one
   * tile per turn. Both are free by-products of the loop, neither
   * changes a decision, and together they say what a bound would
   * remove before anyone picks one. */
  int sea_max_dist;
  int sea_far;
};

/**********************************************************************//**
  Emit the wave census beside the verdict it produced.

  What it discriminates: a low survival probability caused by a couple
  of strong attackers standing next door (the threshold is mis-set)
  versus one caused by a reach radius that admits a distant fleet (the
  horizon, or the sea clause below, is wrong). The 2026-08-11 forensics
  spot-checked this from the aggregate `wave=` field alone and read it
  as small waves of local units -- but the sea && coastal arm of the
  reachability test has NO distance bound beyond horizon x move_rate,
  which for anything moving 6 exceeds the diameter of a 32x32 torus.
  max_dist and by_sea are what settle it, and since the move-type fix
  made that arm live, sea_far and sea_max_dist say how much of it a
  bound would actually remove.

  THE PATHFINDER'S OPINION (ERA 3c instrument). The census above
  measured the reach and the 2026-08-12 probe priced it: by t>130,
  58.1% of survival verdicts include an attacker at least 12 tiles
  away, admitted by `real_map_distance / move_rate` with no terrain
  cost, no ZOC, no fog and no pathfinder. That is a candidate cause of
  deep's over-fearfulness -- but the probe showed only the REACH, not
  that the reach is wrong, and the evidence document is explicit that
  guessing here is the failure mode 00-context.md section 5 names.
  So: measure the gap before touching the test.

  `pf_turns` is the pathfinder's own turn count for the SINGLE
  furthest qualifying attacker, on the same attack parameter
  find_something_to_kill() uses (daiunit.c:603), against `far_turns`,
  that same unit's straight-line verdict. -1 means the pathfinder says
  the tile is unreachable at all -- the strongest possible form of the
  gap, and the one worth counting.

  COST, stated plainly: this is one pf_map_new() per survival
  evaluation, and survival evaluation is the hottest routine deep has
  (437 440 calls over 32 probe games). A pf map for every qualifying
  attacker -- the reading that would let the gap be attributed per
  route -- is 4.8x that and was not affordable. One map for the
  furthest attacker is, because the furthest attacker is exactly the
  one the 3c question is about, and because pf_map_position() stops
  expanding when it reaches the city rather than filling the map.
  Even so the call is gated on the metric log level, the same way
  deep_opodds_log()'s defence census is: under FREECIV_DEEP_QUIET a
  tuning run pays nothing at all.
**************************************************************************/
static void deep_wave_log(const struct city *pcity, int horizon,
                          int n_att, int n_def,
                          const struct deep_combatant *att,
                          const struct deep_combatant *def,
                          const struct deep_wave_census *census,
                          double surv)
{
  int pf_turns = -1;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }
  if (census->far_unit != NULL) {
    struct pf_parameter parameter;
    struct pf_map *pfm;
    struct pf_position pos;

    pft_fill_unit_attack_param(&parameter, &(wld.map),
                               (struct unit *) census->far_unit);
    pfm = pf_map_new(&parameter);
    if (pf_map_position(pfm, city_tile(pcity), &pos)) {
      pf_turns = pos.turn;
    }
    pf_map_destroy(pfm);
  }

  log_base(deep_metric_log_level(),
           "deep-wave turn=%d player=\"%s\" city=%d n_att=%d n_def=%d"
           " capped=%d by_land=%d by_sea=%d by_air=%d by_transport=%d"
           " sea_far=%d sea_max_dist=%d"
           " max_att=%d max_dist=%d max_turns=%d horizon=%d"
           " far_dist=%d far_turns=%d pf_turns=%d"
           " net_pinned=%d net_marching=%d launch=%.3f"
           " def_best=%d surv=%.3f",
           game.info.turn, player_name(city_owner(pcity)), pcity->id,
           n_att, n_def, census->n_qual > n_att ? 1 : 0,
           census->by_land, census->by_sea, census->by_air,
           census->by_transport,
           census->sea_far, census->sea_max_dist,
           n_att > 0 ? att[0].rating : 0,
           census->max_dist, census->max_turns, horizon,
           census->far_dist, census->far_turns, pf_turns,
           census->net_pinned, census->net_marching,
           deep_op_launch_rate(city_owner(pcity)),
           n_def > 0 ? def[0].rating : 0, surv);
}

/* THE GARRISON-PIN PROBE (NETTHREAT item 2's cheaper probe).
 *
 * Item 2 wants to DISCOUNT the wave by the enemy units that are
 * pinned at home: "units whose own city would fall without them are
 * unlikely wave members". That claim is falsifiable without writing
 * the discount, and this is the falsifier. Every enemy unit that
 * qualifies for a wave while standing in one of its OWNER's cities is
 * tagged, and the following `horizon` turns are watched: does it ever
 * leave, does it ever enter our borders, does it ever reach a tile
 * from which it could hit one of our cities?
 *
 * NO BEHAVIOUR. Nothing reads a tag; the whole probe is gated on the
 * metric log level, so a tuning run under FREECIV_DEEP_QUIET does not
 * allocate a byte. The only output is the `deep-pin` line.
 *
 * WHAT "ATTACKED US" IS MEASURED AS, and why the substitution is the
 * conservative one. There is no combat seam in the AI vtable -- no
 * hook fires when a foreign unit attacks us, only `unit_move_seen`
 * when one moves -- so a literal attack count is not observable from
 * inside `ai/deep`. What IS observable is arrival: a tagged unit seen
 * within one tile of a city of the tagging player is a unit that can
 * hit that city on its next move, which is precisely what wave
 * membership asserts. Arrival is a SUPERSET of attacking (a unit may
 * arrive and not strike), so it can only overstate the pin rate --
 * and the verdict this probe exists to give is "the discount is
 * honest if the rate is LOW". A superset that reads low is a real
 * answer; a superset that reads high refuses the discount on the
 * strictest available reading. The error is in the safe direction
 * either way, which is the only reason a proxy is acceptable here.
 *
 * Two observation points, because one is not enough. The per-turn
 * poll at phase begin sees where each tagged unit stands; the
 * `unit_move_seen` hook catches the units that sortie, act and die
 * inside a single foreign phase, which the poll would score as "never
 * left home". Together they miss only a unit that attacks from a tile
 * it was already standing on and never moves -- and that unit is
 * inside our reach radius already, so its `near` flag was set when it
 * arrived there.
 *
 * ONE TAG PER UNIT AT A TIME, first observer keeps it, re-taggable
 * once its window closes. That makes the sample a set of independent
 * (unit, window) observations rather than a count weighted by how
 * many of our cities happened to fear the same unit. */

#define DEEP_PIN_BUCKETS 512

struct deep_pin_tag {
  int unit_id;
  int next;             /* bucket chain and free list; -1 ends both */
  int observer;         /* player slot of the city owner that tagged */
  int city_id;
  int home_tile;        /* the city tile it garrisoned when tagged */
  int tag_turn;
  int horizon;
  bool live;
  bool best;            /* it was that city's best defender */
  bool alone;           /* it was that city's ONLY military unit */
  bool left;            /* seen off its home tile */
  bool terr;            /* seen inside the observer's borders */
  bool near;            /* seen within one tile of an observer city */
};

/* Per observer, cumulative over the game. `res` is the denominator of
 * every rate; `near` is the pin-rate numerator. */
struct deep_pin_count {
  int tagged, res, left, terr, near, home, dead;
  int best, best_near, alone, alone_near;
};

static struct deep_pin_tag *deep_pin_tags = NULL;
static int deep_pin_n = 0, deep_pin_cap = 0;
static int deep_pin_bucket[DEEP_PIN_BUCKETS];
static int deep_pin_free = -1;
static bool deep_pin_ready = FALSE;
static struct deep_pin_count deep_pin_stat[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  Drop every tag and every count. Called when the game is freed, so a
  second game in the same server process never reads the first one's
  units -- unit ids are recycled and a stale tag would be attached to
  whatever inherited the number.
**************************************************************************/
void deep_pin_reset(void)
{
  free(deep_pin_tags);
  deep_pin_tags = NULL;
  deep_pin_n = 0;
  deep_pin_cap = 0;
  deep_pin_free = -1;
  deep_pin_ready = FALSE;
  memset(deep_pin_stat, 0, sizeof(deep_pin_stat));
}

/**********************************************************************//**
  The open tag for this unit, or NULL. O(1): the bucket chain is what
  keeps the per-attacker test in the hottest routine deep has from
  being a scan over every open tag.
**************************************************************************/
static struct deep_pin_tag *deep_pin_find(int unit_id)
{
  int i;

  if (!deep_pin_ready) {
    return NULL;
  }
  for (i = deep_pin_bucket[unit_id & (DEEP_PIN_BUCKETS - 1)];
       i >= 0; i = deep_pin_tags[i].next) {
    if (deep_pin_tags[i].unit_id == unit_id) {
      return &deep_pin_tags[i];
    }
  }

  return NULL;
}

/**********************************************************************//**
  Record where a tagged unit was seen. Called from the per-turn poll
  and from every move the server reports, and idempotent in both --
  the flags only ever go from false to true, so the two observers
  cannot double-count.
**************************************************************************/
static void deep_pin_observe(struct deep_pin_tag *t,
                             const struct unit *punit)
{
  struct tile *ptile = unit_tile(punit);
  const struct player *obs = player_by_number(t->observer);

  if (obs == NULL || ptile == NULL) {
    return;
  }
  if (tile_index(ptile) != t->home_tile) {
    t->left = TRUE;
  }
  if (tile_owner(ptile) == obs) {
    t->terr = TRUE;
  }
  if (!t->near) {
    square_iterate(&(wld.map), ptile, 1, atile) {
      const struct city *acity = tile_city(atile);

      if (acity != NULL && city_owner(acity) == obs) {
        t->near = TRUE;
      }
    } square_iterate_end;
  }
}

/**********************************************************************//**
  Close a tag: fold it into its observer's counts and return the slot
  to the free list. `dead` means the unit was gone from the game
  before its window expired -- counted apart, because a unit that died
  at home is evidence for the pin and a unit that died attacking is
  evidence against it, and the flags say which.
**************************************************************************/
static void deep_pin_resolve(int idx, bool dead)
{
  struct deep_pin_tag *t = &deep_pin_tags[idx];
  struct deep_pin_count *c = &deep_pin_stat[t->observer];
  int b = t->unit_id & (DEEP_PIN_BUCKETS - 1);
  int i, prev = -1;

  c->res++;
  if (dead) {
    c->dead++;
  }
  if (t->left) {
    c->left++;
  } else {
    c->home++;
  }
  if (t->terr) {
    c->terr++;
  }
  if (t->near) {
    c->near++;
  }
  if (t->best) {
    c->best++;
    if (t->near) {
      c->best_near++;
    }
  }
  if (t->alone) {
    c->alone++;
    if (t->near) {
      c->alone_near++;
    }
  }

  for (i = deep_pin_bucket[b]; i >= 0; i = deep_pin_tags[i].next) {
    if (i == idx) {
      if (prev < 0) {
        deep_pin_bucket[b] = t->next;
      } else {
        deep_pin_tags[prev].next = t->next;
      }
      break;
    }
    prev = i;
  }
  t->live = FALSE;
  t->unit_id = 0;
  t->next = deep_pin_free;
  deep_pin_free = idx;
}

/**********************************************************************//**
  Tag one qualifying attacker that is standing in a city of its own.

  Cost, because this sits in the wave-assembly loop: one bucket lookup
  per garrisoning attacker per evaluation, and the rest of the work
  only on the evaluation that opens the window. The `best` / `alone`
  pass over the city tile is therefore paid once per unit per horizon,
  not once per evaluation.
**************************************************************************/
static void deep_pin_open(struct player *observer, const struct city *pcity,
                          struct unit *aunit, const struct city *acity,
                          int horizon)
{
  struct deep_pin_tag *t;
  int rating = adv_unit_def_rating_basic(aunit);
  int idx, others = 0, stronger = 0;

  if (rating <= 0 || deep_pin_find(aunit->id) != NULL) {
    /* Not a would-be defender, or already inside an open window. */
    return;
  }
  if (!deep_pin_ready) {
    int b;

    for (b = 0; b < DEEP_PIN_BUCKETS; b++) {
      deep_pin_bucket[b] = -1;
    }
    deep_pin_ready = TRUE;
  }
  if (deep_pin_free >= 0) {
    idx = deep_pin_free;
    deep_pin_free = deep_pin_tags[idx].next;
  } else {
    if (deep_pin_n >= deep_pin_cap) {
      deep_pin_cap = MAX(256, deep_pin_cap * 2);
      deep_pin_tags = fc_realloc(deep_pin_tags,
                                 deep_pin_cap * sizeof(*deep_pin_tags));
    }
    idx = deep_pin_n++;
  }

  unit_list_iterate(city_tile(acity)->units, ounit) {
    if (ounit == aunit
        || unit_owner(ounit) != city_owner(acity)
        || unit_has_type_flag(ounit, UTYF_CIVILIAN)
        || ounit->hp <= 0) {
      continue;
    }
    others++;
    if (adv_unit_def_rating_basic(ounit) >= rating) {
      stronger++;
    }
  } unit_list_iterate_end;

  t = &deep_pin_tags[idx];
  t->unit_id = aunit->id;
  t->observer = player_index(observer);
  t->city_id = pcity->id;
  t->home_tile = tile_index(unit_tile(aunit));
  t->tag_turn = game.info.turn;
  t->horizon = horizon;
  t->live = TRUE;
  t->best = (stronger == 0);
  t->alone = (others == 0);
  t->left = FALSE;
  t->terr = FALSE;
  t->near = FALSE;
  t->next = deep_pin_bucket[t->unit_id & (DEEP_PIN_BUCKETS - 1)];
  deep_pin_bucket[t->unit_id & (DEEP_PIN_BUCKETS - 1)] = idx;
  deep_pin_stat[t->observer].tagged++;
}

/**********************************************************************//**
  Is the pin tracker wanted at all? Two consumers: the `deep-pin`
  probe line (whenever the metric log level is emitted, as always),
  and -- since NETTHREAT item 2 turned the probe's observation into a
  discount -- the wave weighting itself whenever threat_pin_discount
  is nonzero. Without the second arm the knob would read an EMPTY
  tracker in every FREECIV_DEEP_QUIET run, which is every tuning run:
  a knob that silently switches off in the harness that sweeps it.

  The classification the tracker makes is untouched; this only says
  when it runs. Read through deep_tuning_get() so the phase-scoped
  seat decides; the move hook fires unscoped and reads the base
  tuning, which is exact for every single-file (TUNING=) run and
  conservative-off only for a per-seat arena knob nobody sweeps
  quiet.
**************************************************************************/
static bool deep_pin_track_active(void)
{
  return log_do_output_for_level(deep_metric_log_level())
         || deep_tuning_get()->threat_pin_discount > 0.0;
}

/**********************************************************************//**
  A unit moved somewhere the server could see. The second observation
  point: a garrison that sorties, strikes and dies inside one foreign
  phase never appears away from home in any poll, and without this it
  would be counted as a unit that never left.
**************************************************************************/
void deep_pin_note_move(struct unit *punit)
{
  struct deep_pin_tag *t;

  if (deep_pin_n == 0 || !deep_pin_track_active()) {
    return;
  }
  t = deep_pin_find(punit->id);
  if (t != NULL && t->live) {
    deep_pin_observe(t, punit);
  }
}

/**********************************************************************//**
  This player's turn to look at its own tags: observe every one that
  is still open, close the ones whose horizon has run out, and report
  the running funnel.

  `deep-pin` reads tagged -> left -> terr -> near, each a strictly
  weaker claim than the last about whether a garrisoning enemy turned
  out to be a wave member. `rate` is near/res: the fraction of units
  that were garrisoning when we sized a garrison against them and that
  did reach a position to hit one of our cities inside the horizon we
  sized for. THAT number decides whether item 2's discount is honest.
**************************************************************************/
void deep_pin_poll(struct player *pplayer)
{
  const struct deep_pin_count *c;
  int slot = player_index(pplayer);
  int i, open = 0;

  if (deep_pin_n == 0 || !deep_pin_track_active()) {
    return;
  }
  for (i = 0; i < deep_pin_n; i++) {
    struct deep_pin_tag *t = &deep_pin_tags[i];
    struct unit *punit;

    if (!t->live) {
      continue;
    }
    if (t->observer != slot) {
      const struct player *owner = player_by_number(t->observer);

      /* A dead observer will never poll again, and its unresolved
       * tags would block those units from ever being tagged by
       * anyone else. Close them where they stand. */
      if (owner == NULL || !owner->is_alive) {
        deep_pin_resolve(i, FALSE);
      }
      continue;
    }
    punit = game_unit_by_number(t->unit_id);
    if (punit == NULL) {
      deep_pin_resolve(i, TRUE);
      continue;
    }
    deep_pin_observe(t, punit);
    if (game.info.turn - t->tag_turn >= t->horizon) {
      deep_pin_resolve(i, FALSE);
    } else {
      open++;
    }
  }

  c = &deep_pin_stat[slot];
  if (c->tagged <= 0) {
    return;
  }
  log_base(deep_metric_log_level(),
           "deep-pin turn=%d player=\"%s\" tagged=%d open=%d res=%d"
           " left=%d terr=%d near=%d home=%d dead=%d"
           " best=%d best_near=%d alone=%d alone_near=%d rate=%.3f",
           game.info.turn, player_name(pplayer), c->tagged, open, c->res,
           c->left, c->terr, c->near, c->home, c->dead,
           c->best, c->best_near, c->alone, c->alone_near,
           c->res > 0 ? (double) c->near / c->res : 0.0);
}

/* THE DISCOUNT'S OWN LEDGER (NETTHREAT item 2). Cumulative per
 * observer, like the probe's funnel: every survival evaluation of a
 * deep city while the knob is on adds its admitted rating mass to
 * `wave_mass`, and every attacker the pin classification weighed down
 * adds itself to `units` and its rating to `mass` (before) and
 * `mass_after` (after). The line this feeds exists to answer exactly
 * one question -- DID THE SEAM REACH THE POPULATION -- because a
 * knob-on run where share stays ~0 means the discount is priced on
 * units the tracker never tags, and that run must be reported, not
 * shipped. */
struct deep_pindisc_count {
  int evals;                    /* evaluations while the knob was on */
  int hit;                      /* ... with >= 1 discounted attacker */
  int units;                    /* discounted (attacker, evaluation)s */
  double mass;                  /* their rating mass, undiscounted */
  double mass_after;            /* the same mass after (1 - discount) */
  double wave_mass;             /* ALL admitted rating mass */
};

static struct deep_pindisc_count deep_pindisc_stat[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  Drop the discount ledger. Beside deep_pin_reset() in the game-free
  hook, for the same reason: a second game in the same server process
  starts from zero.
**************************************************************************/
void deep_pindisc_reset(void)
{
  memset(deep_pindisc_stat, 0, sizeof(deep_pindisc_stat));
}

/**********************************************************************//**
  The running ledger, one line per player per turn beside `deep-pin`.
  share is the fraction of ALL rating mass that entered this player's
  waves and was then discounted away -- the number the brief says must
  be visibly nonzero for the knob to be honest.
**************************************************************************/
void deep_pindisc_log(struct player *pplayer)
{
  const struct deep_pindisc_count *c
    = &deep_pindisc_stat[player_index(pplayer)];

  if (c->evals <= 0) {
    return;
  }
  log_base(deep_metric_log_level(),
           "deep-pindisc turn=%d player=\"%s\" evals=%d hit=%d units=%d"
           " mass=%.1f mass_after=%.1f wave_mass=%.1f share=%.4f",
           game.info.turn, player_name(pplayer), c->evals, c->hit,
           c->units, c->mass, c->mass_after, c->wave_mass,
           c->wave_mass > 0.0
             ? (c->mass - c->mass_after) / c->wave_mass : 0.0);
}

/**********************************************************************//**
  As deep_city_stack_survival(), with one garrison unit left out of
  the stack -- what the night costs if that blade sorties or is lent.
**************************************************************************/
static double deep_city_survival_core(const struct civ_map *nmap,
                                      struct city *pcity, int horizon,
                                      const struct unit *except,
                                      struct unit *const *extra, int n_extra,
                                      int *wave_out,
                                      struct deep_defence_read *out)
{
  struct deep_combatant att[DEEP_WAVE_MAX], def[DEEP_STACK_MAX];
  int n_att = 0, n_def;
  struct player *pplayer = city_owner(pcity);
  struct tile *ctile = city_tile(pcity);
  Continent_id home = tile_continent(ctile);
  bool coastal = is_terrain_class_near_tile(nmap, ctile, TC_OCEAN);
  int n_air = 0;
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_wave_census census = { 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0,
                                     0, 0, 0, 0 };
  /* NETTHREAT item 1's instrument: the enemy cities our own operations
   * point at, gathered once per evaluation. Empty for a player with no
   * live operation, which is most of them most of the time, so the
   * per-attacker test below costs nothing in the common case. */
  int op_targets[DEEP_OPS_MAX], op_marching[DEEP_OPS_MAX];
  int n_targets = deep_op_live_targets(pplayer, op_targets,
                                       DEEP_OPS_MAX, FALSE);
  int n_marching = deep_op_live_targets(pplayer, op_marching,
                                        DEEP_OPS_MAX, TRUE);
  /* NETTHREAT item 2, hoisted out of the loop: one log-level test and
   * one ai-type test per evaluation rather than per attacker. The
   * probe now has a second reason to run -- the discount reads its
   * tags -- so it is armed by the metric log level OR the knob, and
   * still false in every knob-off tuning run, which is what keeps the
   * hot path free where it is hottest.
   *
   * The discount itself applies only when the city being defended is
   * a deep player's own: that is "deep's danger assessment". The same
   * core pricing an ENEMY city for the assault side keeps full-weight
   * attackers. */
  bool pin_probe = (pplayer->ai == deep_ai_get_self()
                    && (log_do_output_for_level(deep_metric_log_level())
                        || tune->threat_pin_discount > 0.0));
  double pin_disc = (pplayer->ai == deep_ai_get_self()
                     && tune->threat_pin_discount > 0.0)
                    ? MIN(tune->threat_pin_discount, 1.0) : 0.0;
  bool pin_hit = FALSE;

  if (wave_out != NULL) {
    *wave_out = 0;
  }
  if (out != NULL) {
    out->surv = 1.0;
    out->wave = 0;
    out->n_def = 0;
    out->att_loss = 0.0;
    out->hits = 0.0;
  }

  players_iterate_alive(aplayer) {
    /* The initial diplstate between unmet players is war; fearing
     * their starting units pre-contact is insurance bought before
     * evidence (the ramp law). Barbarians never make contact -- their
     * units on the map ARE the evidence. */
    if (!pplayers_at_war(pplayer, aplayer)
        || (!is_barbarian(aplayer)
            && player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT)) {
      continue;
    }
    unit_list_iterate(aplayer->units, aunit) {
      const struct unit_type *at = unit_type_get(aunit);
      int rating = adv_unit_att_rating(aunit);
      int rate, turns, dist;
      bool flies_in;

      if (rating <= 0 || aunit->hp <= 0) {
        continue;
      }
      /* Reachable: shares the continent, is already at sea aboard a
       * transport, is a warship and the city has a shore -- or it
       * flies, and we are inside its reach.
       *
       * The air term is combat E2 (military audit G5). Every enemy
       * unit that neither swam nor rode used to be dropped for being
       * on another landmass, which silently means the survival model
       * cannot see a paratrooper or a helicopter at all: the two
       * capabilities in the shipped rulesets whose whole point is
       * that no approach is visible. Reach is the drop range or the
       * move rate, from the same class test and the same sizing the
       * garrison floor's airborne class uses -- so the floor that
       * insures against air and the wave that would price it agree by
       * construction. */
      flies_in = tile_continent(unit_tile(aunit)) != home
                      && !unit_transported(aunit)
                      && deep_utype_is_airborne(at)
                      && real_map_distance(unit_tile(aunit), ctile)
                         <= deep_utype_air_reach(at);

      if (tile_continent(unit_tile(aunit)) != home
          && !unit_transported(aunit)
          && !(deep_utype_is_sea(at) && coastal)
          && !flies_in) {
        continue;
      }
      if (flies_in) {
        n_air++;
      }
      rate = MAX(at->move_rate, 1);
      dist = real_map_distance(unit_tile(aunit), ctile);
      turns = (dist * SINGLE_MOVE + rate - 1) / rate;
      if (turns > horizon) {
        continue;
      }
      /* HONEST LAND REACH (ERA 3c): confirm the straight-line
       * admission with the pathfinder. Land arm only -- the probe
       * measured the gap there (19.1% unreachable, 25.6% slower,
       * never faster); the sea arm admitted zero units in 437 440
       * evaluations, the air arm prices a reach that IS the point of
       * flight, and a passenger's approach belongs to its hull.
       * Because the pathfinder is never faster than the straight
       * line on the measured venue, the test above remains a free
       * prefilter and this clause only removes attackers. */
      if (tune->survival_pf_reach > 0
          && !flies_in
          && !unit_transported(aunit)
          && tile_continent(unit_tile(aunit)) == home) {
        int pf_turns = deep_reach_pf_turns(nmap, aunit, ctile);

        if (pf_turns < 0 || pf_turns > horizon) {
          continue;
        }
      }
      /* Census (ERA), on the same four arms the test above just
       * evaluated and in its order: flight first, then own landmass,
       * then a passenger aboard a hull, and whatever is left reached
       * us as a warship against a shore. */
      if (flies_in) {
        census.by_air++;
      } else if (tile_continent(unit_tile(aunit)) == home) {
        census.by_land++;
      } else if (unit_transported(aunit)) {
        census.by_transport++;
      } else {
        census.by_sea++;
        census.sea_max_dist = MAX(census.sea_max_dist, dist);
        if (dist > horizon) {
          census.sea_far++;
        }
      }
      census.n_qual++;
      /* OFFENSE CONSUMES THE WAVE (NETTHREAT item 1), measured only.
       * Is this qualifying attacker standing in a city one of our own
       * operations is aimed at? Those are the wave members our
       * offense is about to engage -- the mass the discount could
       * ever net -- and the MARCH/STRIKE split is what says how much
       * of it belongs to a wave that is actually moving. NOTHING is
       * subtracted: the discount needs this census and the launch
       * rate first, and a wave discounted by intents is the
       * disarmament spiral the brief names. */
      if (n_targets > 0) {
        const struct city *acity = tile_city(unit_tile(aunit));

        if (acity != NULL) {
          int t;

          for (t = 0; t < n_targets; t++) {
            if (op_targets[t] == acity->id) {
              census.net_pinned++;
              break;
            }
          }
          for (t = 0; t < n_marching; t++) {
            if (op_marching[t] == acity->id) {
              census.net_marching++;
              break;
            }
          }
        }
      }
      /* THE GARRISON PIN (NETTHREAT item 2), measured only. This
       * qualifying attacker is standing in one of its OWNER's cities,
       * so item 2 would call it a poor wave member and discount it.
       * Open a window on it instead and let the next `horizon` turns
       * say whether it ever comes. Only a deep player's own city
       * tags: the core also prices ENEMY cities for the assault
       * pricer (deepops.c), and an observer that is not us would put
       * the funnel on the wrong side of the war. */
      if (pin_probe) {
        const struct city *acity = tile_city(unit_tile(aunit));

        if (acity != NULL && city_owner(acity) == aplayer) {
          deep_pin_open(pplayer, pcity, aunit, acity, horizon);
        }
      }
      census.max_dist = MAX(census.max_dist, dist);
      census.max_turns = MAX(census.max_turns, turns);
      /* Keep the furthest qualifying attacker itself, so the ERA 3c
       * instrument can ask the pathfinder about the SAME unit whose
       * straight-line verdict admitted it. */
      if (census.far_unit == NULL || dist > census.far_dist) {
        census.far_unit = aunit;
        census.far_dist = dist;
        census.far_turns = turns;
      }
      /* THE GARRISON-PIN DISCOUNT (NETTHREAT item 2), the one seam
       * where it acts. An attacker the tracker currently classifies
       * as home-pinned -- open window, never yet seen off the city
       * tile it garrisoned -- enters the wave at presence
       * (1 - threat_pin_discount) instead of at certainty; everything
       * else enters at 1.0 exactly as before. Placed AFTER the
       * deep_pin_open() call above so a garrison first seen this
       * evaluation is tagged and then weighed in the same pass. */
      {
        double w = 1.0;

        if (pin_disc > 0.0) {
          struct deep_pindisc_count *pd
            = &deep_pindisc_stat[player_index(pplayer)];
          const struct deep_pin_tag *t = deep_pin_find(aunit->id);

          if (t != NULL && t->live && !t->left) {
            w = 1.0 - pin_disc;
            pin_hit = TRUE;
            pd->units++;
            pd->mass += rating;
            pd->mass_after += w * rating;
          }
          pd->wave_mass += rating;
        }
        deep_combatant_add(att, &n_att, DEEP_WAVE_MAX, aunit, rating, w);
      }
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  if (pin_disc > 0.0) {
    struct deep_pindisc_count *pd
      = &deep_pindisc_stat[player_index(pplayer)];

    pd->evals++;
    if (pin_hit) {
      pd->hit++;
    }
  }

  if (n_att == 0) {
    return 1.0;
  }
  if (wave_out != NULL) {
    *wave_out = n_att;
  }
  if (n_air > 0) {
    /* Its own counter, logged only when the term has a population:
     * zero lines in a cell is the proof that the cell never reached
     * flight, which is what both benchmark cells did before this. */
    log_base(deep_metric_log_level(),
             "deep-air turn=%d player=\"%s\" city=%d wave=%d air=%d"
             " horizon=%d",
             game.info.turn, player_name(pplayer), pcity->id, n_att,
             n_air, horizon);
  }

  n_def = deep_collect_defenders(pcity, def, except);
  {
    int e;

    for (e = 0; e < n_extra; e++) {
      deep_combatant_add(def, &n_def, DEEP_STACK_MAX, extra[e],
                         adv_unit_def_rating_basic(extra[e]), 1.0);
    }
    if (n_extra > 0) {
      qsort(def, n_def, sizeof(def[0]), deep_combatant_cmp);
    }
  }
  qsort(att, n_att, sizeof(att[0]), deep_combatant_cmp);

  if (out == NULL) {
    double surv = deep_garrison_vs_stream(nmap, att, n_att, def, n_def,
                                          NULL);

    deep_wave_log(pcity, horizon, n_att, n_def, att, def, &census, surv);

    return surv;
  }

  {
    double death[DEEP_WAVE_MAX];
    double surv, shields = 0.0;
    int j;

    surv = deep_garrison_vs_stream_full(nmap, att, n_att, def, n_def,
                                        death, &(out->hits));
    for (j = 0; j < n_att; j++) {
      shields += death[j] * unit_build_shield_cost_base(att[j].punit);
    }
    out->surv = surv;
    out->wave = n_att;
    out->n_def = n_def;
    out->att_loss = shields;
    deep_wave_log(pcity, horizon, n_att, n_def, att, def, &census, surv);

    return surv;
  }
}

/**********************************************************************//**
  The garrison's survival without one of its own blades (the sortie
  and lending gates: what does this city's night look like if this
  unit leaves?).
**************************************************************************/
double deep_city_stack_survival_without(const struct civ_map *nmap,
                                        struct city *pcity, int horizon,
                                        const struct unit *except,
                                        int *wave_out)
{
  return deep_city_survival_core(nmap, pcity, horizon, except, NULL, 0,
                                 wave_out, NULL);
}

/**********************************************************************//**
  The garrison's survival WITH reinforcements that are not standing
  here yet: what this city's night looks like if these blades arrive
  in time. The defence counter-claim sizes a relief force this way --
  send enough or send none, the same massing discipline the operations
  layer imposes on attacks.
**************************************************************************/
double deep_city_stack_survival_with(const struct civ_map *nmap,
                                     struct city *pcity, int horizon,
                                     struct unit *const *extra, int n_extra,
                                     int *wave_out)
{
  return deep_city_survival_core(nmap, pcity, horizon, NULL, extra, n_extra,
                                 wave_out, NULL);
}

/**********************************************************************//**
  Both counterfactuals in one read: this blade is gone AND these
  reinforcements stand here. What the night looks like after a
  REARRANGEMENT rather than after a removal -- the question the
  hole-covering price asks (NETTHREAT item 4).

  The core has taken both arguments since it was written; nothing here
  is new arithmetic, only a caller that needs the pair.
**************************************************************************/
double deep_city_stack_survival_swap(const struct civ_map *nmap,
                                     struct city *pcity, int horizon,
                                     const struct unit *except,
                                     struct unit *const *extra, int n_extra,
                                     int *wave_out)
{
  return deep_city_survival_core(nmap, pcity, horizon, except, extra,
                                 n_extra, wave_out, NULL);
}

/**********************************************************************//**
  THE DEFENSIVE LEDGER: the same wave, the same chained duels, and both
  sides of what they cost.

  Everything in this module until now reported only P(hold). That is
  half the answer, and it is the half that prices the CITY. The other
  half prices the GARRISON: a defender's worth is not only whether it
  survives the wave, it is the attacker shields it destroys on the way
  down and the attacks it makes the enemy spend. A musket behind walls
  that loses is still worth what it killed (human, 2026-08-06).

  - att_loss: E[attacker shields destroyed], summed over the wave as
    P(this attacker dies) x its build cost. Same currency as
    deep_take_odds_cost()'s loss_shields, from the other side of the
    line.
  - hits: E[successful enemy attacks] == E[defenders killed], the
    citizen bill under killcitizen.
  - surv, wave, n_def as usual.

  `extra` lets a caller ask the counterfactual "what if THIS unit were
  also standing here" -- which is how the marginal value of one more
  defender is priced rather than guessed.

  Inherits every assumption the substrate ships with (one wave, no
  healing, no promotion, no reinforcement between duels, ground truth
  rather than observation) and adds none.
**************************************************************************/
void deep_city_defence_read(const struct civ_map *nmap, struct city *pcity,
                            int horizon, struct unit *const *extra,
                            int n_extra, struct deep_defence_read *out)
{
  (void) deep_city_survival_core(nmap, pcity, horizon, NULL, extra,
                                 n_extra, NULL, out);
}

/**********************************************************************//**
  The other direction of the same arithmetic: the chance that OUR
  ordered stream of attackers takes acity's garrison -- one minus the
  garrison's survival. The stream is priced as arriving together and
  fresh; making that simultaneity true is the operations layer's job,
  so build-side consumers should treat this as the ceiling the
  assembled force could reach.
**************************************************************************/
double deep_take_odds(const struct civ_map *nmap, struct city *acity,
                      struct unit *const *stream, int n_stream)
{
  return deep_take_odds_at_arrival(nmap, acity, stream, n_stream, 0);
}

/**********************************************************************//**
  The strongest defender the target could put on the queue right now.
  This is both the unit the reinforcement model copies into the city
  and the denominator of the observed build rate, so the two agree by
  construction rather than by coincidence.

  Omniscient, like everything else this file reads off an enemy city
  (see the assumption list on deep_take_odds_cost() and
  findings/omniscience-census-2026-08-11.md S5). W04's estimator
  replaces the read; the arithmetic on top of it survives that swap.
**************************************************************************/
static const struct unit_type *
deep_best_buildable_defender(const struct civ_map *nmap,
                             const struct city *acity)
{
  const struct unit_type *best = NULL;

  unit_type_iterate(ut) {
    if (ut->defense_strength > 0
        && can_city_build_unit_now(nmap, acity, ut, RPT_CERTAIN)
        && (best == NULL
            || ut->defense_strength > best->defense_strength)) {
      best = ut;
    }
  } unit_type_iterate_end;

  return best;
}

/**********************************************************************//**
  The rate the target city can ACTUALLY replace defenders at, in
  defenders per turn: its shield surplus divided by what the defender
  the model would place costs. Both halves are facts about that city
  on this turn, not constants -- which is the whole point. A
  four-shield frontier town and a war-economy capital have never been
  distinguishable to the reinforcement model, and they differ by an
  order of magnitude.

  Returns 0 when the city cannot build a defender at all, when it runs
  a shield deficit, or when the best defender is free -- all three of
  which mean "this term has nothing to say", never "reinforce fast".

  NOT the whole signal, and the omissions are deliberate: a defender
  BOUGHT with gold arrives in one turn regardless of surplus (the
  buy-response argument in findings/tuning-first-principles), and
  units walking in from a neighbouring city are reinforcement this
  never sees. Both push the true rate up, so the term is a floor on
  the production channel rather than an estimate of all reinforcement,
  and it is registered as a slope precisely so a tuner can price that
  gap instead of a comment guessing at it.
**************************************************************************/
double deep_reinforce_observed(const struct civ_map *nmap,
                               const struct city *acity)
{
  const struct unit_type *best = deep_best_buildable_defender(nmap, acity);
  int cost;

  if (best == NULL) {
    return 0.0;
  }
  cost = utype_build_shield_cost_base(best);
  if (cost <= 0) {
    return 0.0;
  }

  return (double) MAX(acity->surplus[O_SHIELD], 0) / cost;
}

/**********************************************************************//**
  Defenders per turn of our travel time that the target is modelled as
  adding. Two terms:

    invasion_reinforce_rate            a flat rate, the same for every
                                       city on the map
  + invasion_reinforce_slope x observed   what THIS city can build

  invasion_reinforce_slope defaults to 0, which is the flat model this
  shipped with, byte for byte. At 1.0 the observed production rate is
  taken at face value on top of the flat term, so a sweep of the pair
  spans "all cities reinforce alike" through "reinforcement is
  production" without either being assumed.

  The slope rides on the civilised branch only. Barbarians take
  barb_reinforce_rate flat and no slope: T160 forensics found 47 of 52
  barb-held cities garrisonless for ~40 turns, so a barbarian city's
  shield surplus demonstrably does not become defenders, and sloping
  on it would rebuild exactly the phantom-Musketeer wall that knob
  exists to tear down.
**************************************************************************/
double deep_reinforce_rate(const struct civ_map *nmap,
                           const struct city *acity)
{
  const struct deep_tuning *tune = deep_tuning_get();

  /* Barbarians lean against defending their takes (human doctrine
   * + T160 forensics: 47 of 52 barb-held cities garrisonless for
   * ~40 turns while an 87-unit horde raged in the field). Pricing
   * them with the civilised reactive-build model walls off the
   * whole reconquest pool behind phantom Musketeers. */
  if (is_barbarian(city_owner(acity))) {
    return tune->barb_reinforce_rate;
  }

  if (tune->invasion_reinforce_slope <= 0.0) {
    /* The flat model, and not one extra unit_type_iterate per odds
     * call to compute a term that is about to be multiplied by 0. */
    return tune->invasion_reinforce_rate;
  }

  return tune->invasion_reinforce_rate
         + tune->invasion_reinforce_slope
           * deep_reinforce_observed(nmap, acity);
}

/**********************************************************************//**
  Take odds against the garrison AS IT WILL BE when we arrive: travel
  time is the defender's budget (naval design, human refinement
  2026-07-30 -- a 12-turn sail must clear a much higher bar than a
  2-turn hop, which is exactly why long-range desultory attacks fail
  while looking winnable on paper). eta turns of sailing add
  eta x deep_reinforce_rate() virtual copies of the best defender the
  target could build, placed in the city so walls and terrain count.
  eta 0 is the plain current-garrison read.
**************************************************************************/
double deep_take_odds_at_arrival(const struct civ_map *nmap,
                                 struct city *acity,
                                 struct unit *const *stream, int n_stream,
                                 int eta)
{
  return deep_take_odds_cost(nmap, acity, stream, n_stream, eta, NULL,
                             NULL);
}

/**********************************************************************//**
  The take odds AND what taking it is expected to cost: the same walk,
  reporting expected attacker losses in shields and the expected size
  of the force still standing afterwards.

  loss_shields is the sum over the stream of P(this unit dies) times
  its build cost -- the currency the build auction and the granary are
  already priced in, so "is it worth what it costs" is finally a
  subtraction rather than a comparison of scales (military audit G1).
  survivors is the same walk's other half: what would garrison the
  city we just took (the hold-the-city test).

  Both inherit every assumption the substrate ships with, stated in
  findings "Combat workstream epic 1": one wave, no healing, no
  promotion, no reinforcement between duels, military garrison only,
  an omniscient read with no visibility test of any kind (RC6; see
  findings/omniscience-census-2026-08-11.md S5), and the 32/16
  computational caps -- a stream longer than 32 is priced on its 32
  strongest, and the rest are charged nothing.
**************************************************************************/
double deep_take_odds_cost(const struct civ_map *nmap, struct city *acity,
                           struct unit *const *stream, int n_stream,
                           int eta, double *loss_shields,
                           double *survivors)
{
  struct deep_combatant att[DEEP_WAVE_MAX], def[DEEP_STACK_MAX];
  int n_att = 0, n_def, j, reinforce = 0;
  struct unit *virt[DEEP_STACK_MAX];
  int n_virt = 0;
  double survival;
  double death[DEEP_WAVE_MAX];
  bool want_cost = (loss_shields != NULL || survivors != NULL);

  if (loss_shields != NULL) {
    *loss_shields = 0.0;
  }
  if (survivors != NULL) {
    *survivors = 0.0;
  }

  for (j = 0; j < n_stream; j++) {
    deep_combatant_add(att, &n_att, DEEP_WAVE_MAX, stream[j],
                       adv_unit_att_rating(stream[j]), 1.0);
  }
  if (n_att == 0) {
    return 0.0;
  }
  qsort(att, n_att, sizeof(att[0]), deep_combatant_cmp);

  n_def = deep_collect_defenders(acity, def, NULL);

  if (eta > 0) {
    reinforce = (int) (eta * deep_reinforce_rate(nmap, acity) + 0.5);
  }
  if (reinforce > 0) {
    struct player *owner = city_owner(acity);
    const struct unit_type *best
      = deep_best_buildable_defender(nmap, acity);

    while (best != NULL && n_virt < reinforce
           && n_def + n_virt < DEEP_STACK_MAX
           && n_virt < DEEP_STACK_MAX) {
      struct unit *v = unit_virtual_create(owner, acity,
                                           (struct unit_type *) best, 0);

      unit_tile_set(v, city_tile(acity));
      virt[n_virt++] = v;
    }
    for (j = 0; j < n_virt; j++) {
      deep_combatant_add(def, &n_def, DEEP_STACK_MAX, virt[j],
                         adv_unit_def_rating_basic(virt[j]), 1.0);
    }
    qsort(def, n_def, sizeof(def[0]), deep_combatant_cmp);
  }

  survival = deep_garrison_vs_stream(nmap, att, n_att, def, n_def,
                                     want_cost ? death : NULL);

  if (want_cost) {
    double dead = 0.0, shields = 0.0;

    for (j = 0; j < n_att; j++) {
      dead += death[j];
      shields += death[j] * unit_build_shield_cost_base(att[j].punit);
    }
    if (loss_shields != NULL) {
      *loss_shields = shields;
    }
    if (survivors != NULL) {
      *survivors = n_att - dead;
    }
  }

  for (j = 0; j < n_virt; j++) {
    unit_virtual_destroy(virt[j]);
  }

  return 1.0 - survival;
}
