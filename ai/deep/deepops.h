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
#ifndef FC__DEEPOPS_H
#define FC__DEEPOPS_H

/* common */
#include "fc_types.h"

struct ai_type;

/* An operation: the first object in the tree that represents a
 * multi-turn intention (combat E6; W33's substrate). The smash the
 * human asked for: mass troops at a staging city, then launch at a
 * specific goal as one wave instead of the per-turn trickle stock
 * produces. A player runs up to DEEP_OPS_MAX of them at once, each
 * against its own target with its own claimed members.
 *
 * Saved under the deep_state namespace (W33 phase 4) -- benchmarks
 * never reload, but human-game saves do, and an op that dissolved
 * on every reload cost those games their assaults. The enum values
 * below are part of the savegame format: append, never reorder. */
#define DEEP_OP_MEMBERS 24
/* Storage cap on concurrent ARMY operations (assault and raid), like
 * DEEP_OP_MEMBERS a bookkeeping bound rather than a tunable: forming
 * one at all requires a distinct unclaimed target plus enough free
 * attackers to clear op_take_target against it, so the count is
 * self-limiting. */
#define DEEP_ARMY_OPS_MAX 4
/* Extra slots only a lift may occupy. A lift is NOT self-limiting the
 * way an assault is -- it needs one passenger and one spare hull --
 * so it used to consume the army's four slots and then queue behind
 * them: the array was full on 23.3% of player-turns, and full WITH an
 * unclaimed overseas-bound passenger waiting 11.9 times per game
 * (findings/settler-missions.md section 10). How many of these are
 * usable is the tunable lift_op_slots; this is only the storage.
 *
 * Savegame: slots are stored per index with defaulted lookups, so a
 * save written when the array was four long loads into an eight-long
 * one with the new slots reading DEEP_OP_NONE. */
#define DEEP_LIFT_OPS_MAX 4
#define DEEP_OPS_MAX (DEEP_ARMY_OPS_MAX + DEEP_LIFT_OPS_MAX)

/* Phase list (W33 commits 2-3). A land assault runs MUSTER -> MARCH
 * -> STRIKE; launching into MARCH releases the members to the stock
 * attack path while the operation shadows them -- re-gating the take
 * odds at the remaining eta each turn. At contact the op takes them
 * BACK: STRIKE fights the battle under op control, best duel first,
 * every blade on the op target. Amphibious assaults still dissolve
 * at launch: the convoy hold/escort/invade gates own the crossing
 * unchanged. */
enum deep_op_state {
  DEEP_OP_NONE = 0,
  DEEP_OP_MUSTER,
  DEEP_OP_MARCH,
  DEEP_OP_STRIKE,
  /* The city fell to the wave: garrison it from the wave itself,
   * best defender first, until the stack clears the same survival
   * bar every other city answers to -- a capture left open is a
   * recapture (B9-B11 games lost takes this way). */
  DEEP_OP_CONSOLIDATE,
};

/* Op templates (W33). ASSAULT is the muster-and-smash above. RAID
 * (combat E8 graduated into the slot system) claims only fast idle
 * attackers and hunts exposed enemy civilians; it has no target
 * city, no staging, no quorum -- it forms when victims are in reach
 * and dissolves when the hunting stops. Raids coexist with assaults
 * now, but assault formation runs first each turn: the army's first
 * claim on fresh units stands. */
/* LIFT (the colony-lift epic) owns a crossing end to end: a named boat,
 * a named passenger list, a named beachhead, held against re-tasking,
 * with an abort condition. It exists because the ferry pipeline's
 * binding constraint is the RENDEZVOUS, not capacity and not the 1:1
 * cap's arithmetic -- 95.6% of capped waiting unit-turns have a
 * rideable boat with a free slot and only 0.6% ever stand on the same
 * tile, which is where loading happens
 * (findings/settler-missions.md section 9). Stock's two-sided protocol
 * -- passenger raises a flag, boat independently looks for flagged
 * passengers -- is what never closes that gap, so the lift drives the
 * boat itself rather than routing units back into that manager.
 *
 * A lift carries ANY unit that needs to cross: a settler, a worker, or
 * a defensive unit being redeployed to a garrison overseas. Moving
 * troops is not the same intention as attacking with them -- ASSAULT
 * is the amphibious battle, with its own escort and invade gates, and
 * a garrison move must not have to masquerade as one to get a hull. */
enum deep_op_kind {
  DEEP_OPK_ASSAULT = 0,
  DEEP_OPK_RAID,
  DEEP_OPK_LIFT,
};

/* LIVE wait spells (W36, expansion-logistics section 3.1). The wait
 * census (deepops.c) writes one `deep-wait` line per waiting passenger
 * per turn, and a spell is reconstructed OFFLINE -- first-seen there
 * against the turn that unit id appears as a lift's core. That is why
 * nothing in the game can read it, and why site scoring has never
 * priced the queue it sends settlers into. This is the same spell kept
 * live: the turn a unit first wanted a crossing, carried until the
 * wait ends.
 *
 * A spell ENDS when an operation claims the unit (the offline
 * definition: enrolled as a lift's core), when it stops wanting a
 * crossing, or when it dies. On isolated-150, 57.4% of spells end in
 * none of those and simply run out with the game -- the censored tail,
 * and the population the estimate is about.
 *
 * DEEP_WAIT_SPELLS is a bookkeeping bound like DEEP_OP_MEMBERS, not a
 * tunable: past it a further waiter is untracked, which can only
 * understate the queue.
 *
 * Not saved. A reload restarts every spell at the current turn, which
 * understates a wait and can never overstate it; benchmarks never
 * reload. */
#define DEEP_WAIT_SPELLS 32

struct deep_wait_spell {
  int unit_id;                  /* 0 marks a free slot */
  int since;                    /* turn this unit first wanted a crossing */
  int seen;                     /* turn it was last seen still waiting */
};

struct deep_operation {
  enum deep_op_state state;
  enum deep_op_kind kind;
  int target_city;              /* enemy city id */
  int staging_city;             /* own city id */
  int required;                 /* members needed before launch */
  int formed_turn;
  int march_turn;               /* turn the wave launched into MARCH */
  int march_eta;                /* eta to target measured at launch */
  int phase_turn;               /* turn the current phase was entered */
  /* LIFT only: the named boat, and the named landing tile. A lift
   * reuses MUSTER for "the boat is coming to fetch the manifest" and
   * MARCH for "loaded and crossing"; members[] is the manifest. */
  int ferry;                    /* unit id of the boat, 0 if none */
  int beachhead;                /* tile index to land on, -1 if none */
  /* Where boat and manifest have agreed to meet. Named and FIXED at
   * formation on purpose: a passenger walking toward a boat that is
   * simultaneously sailing toward the passenger chases a moving
   * target, and the two can orbit each other for the whole muster. A
   * fixed tile makes the rendezvous a convergence instead. */
  int rendezvous;               /* tile index to meet on, -1 if none */
  /* MUSTER (ERA 3b). A slot that entered MUSTER on the WORTH gate with
   * the odds REFUSED carries `muster = 1` for its whole life, and it is
   * a different object from an ordinary formed wave: it has no
   * `required` quorum (no prefix of the pool ever cleared the bar), it
   * accumulates members every turn, it is never released by a head
   * count, and it is abandoned rather than launched when its target
   * stops being worth taking, its home front needs the force, or it
   * stops growing for op_muster_stall turns.
   *
   * `muster_worth` is the conquest worth at entry, kept for the log so
   * a collapse is legible beside the value that justified the muster.
   * `muster_best` is the high-water member count and `muster_grew` the
   * turn it last rose -- the stall clock's two fields, and the reason
   * the clock measures growth rather than elapsed time.
   *
   * Saved with the rest of the slot, defaulting to 0: a save written
   * before this epic loads as an ordinary wave, which is what it was. */
  int muster;
  int muster_worth;
  int muster_best;
  int muster_grew;
  int n_members;
  int members[DEEP_OP_MEMBERS]; /* unit ids */
};

void deep_run_operations(struct ai_type *ait, struct player *pplayer);

int deep_op_claiming(const struct player *pplayer, int unit_id);
/* Claimed by a live operation aimed at something other than this
 * city: not part of the force that will take it, so the build side
 * must not price its marginal attacker against it. */
bool deep_op_claimed_elsewhere(const struct player *pplayer, int unit_id,
                               int city_id);
/* Is a live operation of ours already committed against this city?
 * O(DEEP_OPS_MAX). Answering it is what lets a decision distinguish a
 * rival's city from ground we are three turns from taking. */
bool deep_op_targeted(const struct player *pplayer, int city_id);
bool deep_op_release_member(const struct player *pplayer, int unit_id);
bool deep_op_holds(const struct player *pplayer, int unit_id);
/* Non-operation holds, scoped to one phase (mechanism-map D-A). A pass
 * that decides a unit should stand still says so here instead of
 * writing unit_ai.done directly, because the manage loop must reset
 * that flag and cannot tell this turn's decision from last turn's. */
void deep_hold_unit(struct ai_type *ait, struct unit *punit,
                    const char *why);
bool deep_phase_held(const struct player *pplayer, int unit_id);
void deep_phase_holds_clear(const struct player *pplayer);
void deep_hold_census(struct ai_type *ait, struct player *pplayer);
/* Capturable-adjacency census, both directions: how much of our own
 * stack surface an enemy capturer could take this instant, and how
 * much of theirs we could. Zero lines in a ruleset without the
 * action. */
void deep_capture_census(struct ai_type *ait, struct player *pplayer);
void deep_manage_units(struct ai_type *ait, struct player *pplayer);
/* Refresh this player's live wait spells; once per player per turn,
 * beside the wait census it makes readable. Observation only. */
void deep_wait_spells_update(struct ai_type *ait, struct player *pplayer);
/* The wait, IN TURNS, that a unit joining the queue on this body of
 * water should expect: the tail of the spells that water is already
 * holding, or 0 where a spare hull is standing free for it. The number
 * site scoring needs and has never had. */
int deep_wait_expected(struct ai_type *ait, struct player *pplayer,
                       Continent_id ocean);
void deep_hull_order_choice(struct ai_type *ait, struct player *pplayer,
                            struct city *pcity,
                            const struct unit_type *ptype);
/* Gate 2's exemption test, shared with gate 5 (deepdefence.c): a
 * barbarian holding, or a hostile city on a landmass where we hold
 * cities, threatens us and keeps the unscaled bar and the unscaled
 * want. One test, two gates -- a second one would be a second law
 * about the same question. */
bool deep_op_target_threatens(const struct player *pplayer,
                              const struct city *target);

/* NETTHREAT item 1's instrument: the enemy cities our live operations
 * point at (MARCH/STRIKE only when `committed_only`, lifts never), and
 * the observed launch rate the discount would have to be weighted by.
 * Read by the survival wave census; nothing acts on them yet. */
int deep_op_live_targets(const struct player *pplayer, int *out, int max,
                         bool committed_only);
double deep_op_launch_rate(const struct player *pplayer);

struct unit *deep_op_poach_defender(struct ai_type *ait,
                                    struct player *pplayer,
                                    struct city *pcity,
                                    double p_survival);

/* What taking this city is worth to us: stock's cached city worth
 * times conquest_worth_mult (take plus denial), plus the culture
 * stock the capture transfers whole. ONE function behind all three
 * conquest seats -- the assault want, the operation's net-value gate
 * and the defence counter-claim -- so the referee cannot differ
 * between them. */
double deep_conquest_worth(struct ai_type *ait, struct player *pplayer,
                           struct city *target);
void deep_conquest_culture_census(struct player *pplayer);

#endif /* FC__DEEPOPS_H */
