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
#ifndef FC__DEEPAI_H
#define FC__DEEPAI_H

/* common */
#include "player.h"

/* ai/default */
#include "daidata.h"

#include "deepops.h"
#include "deeprates.h"
#include "deepstance.h"

struct ai_type;
struct section_file;

struct ai_type *deep_ai_get_self(void);

/* Which threat classes the danger assessment CANNOT SEE are actually
 * live in this game, for this player, this turn. Every field is a
 * server or ruleset FACT -- a setting, a diplomatic state, a unit a
 * rival owns, a unit type a rival's known tech lets it build -- so
 * none of them is a tunable and none of them can be wrong in a way a
 * benchmark would have to discover. Recomputed at most once per player
 * per turn; not saved.
 *
 * The garrison floor is the consumer: insurance against what the
 * assessment misses is worth nothing while the thing it insures
 * against cannot happen yet. */
struct deep_floor_class {
  int turn;                     /* Cache validity, -1 when never filled */
  bool barb;                    /* Server can spawn barbarians NOW */
  bool sea;                     /* A met non-allied rival floats transports */
  bool marines;                 /* ... and can attack off the hull */
  bool air;                     /* A met non-allied rival can build a
                                 * paradrop- or helicopter-class unit */
  bool carrier;                 /* ... and a hull that carries them */
  int air_range;                /* Reach of the best such unit, tiles */
  bool border;                  /* A met non-allied rival holds a city on
                                 * a landmass where we hold one */
};

/* Per-player state of the deep AI. The embedded default-AI state must
 * stay the first member: every forwarded dai_* call finds its data in
 * this module's player slot and casts it to struct ai_plr. */
struct deep_plr {
  struct ai_plr defai;          /* Keep first */

  /* Deep-only state below. Everything here must either be saved in
   * dpai_player_save() or be recomputable. */
  int dv_turns_logged;          /* Decision-vector lines emitted */
  /* What this player is trying to win, as a weight per axis (W34
   * phase A). Refreshed every phase from the tuning file, and saved:
   * a save carries what the player's stance WAS, and a reload with a
   * different tuning file re-resolves it, which is the right
   * behaviour for a control surface. Identity (every axis 1.0) is
   * today's game exactly. */
  struct deep_stance stance;
  /* Gate 1's execution counter: cities whose anticipatory fear was
   * actually scaled by a non-identity stance, cumulative. Four
   * changes in this project have measured as "no effect" while never
   * executing, so a gate that cannot be shown firing is not
   * evidence. Saved, so a reload does not reset the census. */
  int stance_fear_gated;
  /* The same census for gates 2-4 (W34 E34.2): assault formations
   * whose odds bar was actually moved, buildings whose horizon credit
   * was actually scaled, and expansion wants (settler or colony lift)
   * actually scaled. All cumulative, all saved, all zero in the
   * identity arm -- "executed" and "effective" are different claims
   * and only the counter tells them apart. */
  int stance_op_gated;
  int stance_growth_gated;
  int stance_expand_gated;
  /* And for gate 5 (W34 E34.2b): offensive build wants actually
   * scaled -- the assault wants NOT exempted as reconquest of a
   * target that threatens us. Same contract as the other four. */
  int stance_assault_gated;
  /* And for gate 6 (the class-gated garrison floor, 2026-08-05):
     floor injections actually scaled by a non-identity stance. Same
     contract as the other five. */
  int stance_floor_gated;
  /* And for SOURCE 2 (W34 E34.4): turns on which position or research
     lean actually moved an axis off the trait prior it modulates.
     Same contract as the gate counters -- zero at stance_source 0 and
     1, and the only thing that tells "the source is inert" apart from
     "the gains are zero". */
  int stance_pos_moved;
  /* Operation slots (W33); DEEP_OP_NONE marks a free slot. Saved
   * (W33 phase 4): a mid-march wave that dissolved on every reload
   * cost human-game players their assaults; member ids are
   * re-validated at load. */
  struct deep_operation ops[DEEP_OPS_MAX];
  /* Set at load when a marching op came back from a save: member
   * tasks are not saved, so the first operations pass re-points
   * marchers at their target. Not saved itself. */
  bool ops_reissue;
  /* Second-wave bookkeeping (E7): the last launched strike, so the
   * next formation pass can ask "did it stick, and can the reserve
   * plus survivors crush what remains?" Player-level, not per-op:
   * the reserve answers whichever strike was most recent. */
  int last_strike_city;
  int last_strike_turn;
  /* Last turn's war countdown toward each player, so a countdown that
   * LAPSES can be retracted out loud (deeptrade.c has the trade
   * equivalent; this is the diplomacy one). Not saved: after a reload
   * the worst case is one missed retraction, which is strictly better
   * than the current behaviour of never retracting at all. */
  int war_countdown_seen[MAX_NUM_PLAYER_SLOTS];
  /* Live lift-wait spells (W36): who is waiting for a crossing and
   * since when, refreshed once per player per turn by
   * deep_wait_spells_update(). Recomputable rather than saved -- see
   * the contract on struct deep_wait_spell. */
  struct deep_wait_spell wait_spells[DEEP_WAIT_SPELLS];
  /* Colony lift demand (task #20): the best overseas site score a
   * lift-limited settler evaluation recorded, and when. Recomputable
   * -- refreshed every turn the settler pipeline runs. */
  int colony_want;
  int colony_turn;
  /* Berths the hull commission has already ordered THIS turn, and the
   * turn the tally belongs to (EXPANSION slice 2). Every coastal city
   * on a water reads the same gap in the same turn, and the
   * in-production term cannot see an order placed one city ago --
   * measured: 8 cities priced one gap of 2 on the same turn and 5 of
   * them beat their incumbent choice. This is that term extended to
   * orders taken since the queue was counted. Recomputable, not
   * saved, and never touched while the knob is 0. */
  int commission_turn;
  int commission_berths;
  /* Share of the world's land that nobody has claimed yet, 0..1000
   * (per mille, so the cache is an int). deep_map_crowding() and
   * deep_land_pressure() both answer from lobby settings and never
   * move; this is the live figure, and it is what says whether holding
   * a settler is holding an option or holding inventory. Recomputable:
   * refreshed at most once per turn per player. */
  int free_land_permille;
  int free_land_turn;
  /* Threat classes the danger field cannot see, this turn. Cache
   * only: every field is recomputed from server and ruleset facts. */
  struct deep_floor_class floor_class;
  int free_land_tiles;          /* Unowned known land, absolute */
  int free_land_reach;          /* ... of it, on landmasses we stand on */
  int own_land_tiles;           /* Land we own, absolute */
  /* Rates (deeprates.c). rates_tech_want is last turn's price of one
   * research step, snapshotted before dai_clear_tech_wants() wipes the
   * array the rates decision needs; the four counters are the
   * execution census, one per outcome of the override -- rates
   * actually changed, deep agreed with stock, stock was celebrating,
   * and no price was available. Recomputable and cumulative-within-a-
   * run rather than saved: the per-turn deep-rates line is the census
   * the benchmark reads, and a reload that resets the tallies costs a
   * partial count of an already-logged stream. */
  double rates_tech_want;
  /* The split deep actually LEFT IN PLACE last turn: last turn's
   * bulbs, gold and luxury were produced by these rates, not by the
   * ones stock has just re-chosen this turn, and dividing the
   * measurement by the wrong rate is a self-reinforcing trap rather
   * than a rounding error (see deeprates.c). */
  bool rates_applied;
  int rates_last_sci;
  int rates_last_tax;
  int rates_last_lux;
  int rates_set;
  int rates_agreed;
  int rates_celebrate;
  int rates_unpriced;
  /* Government valuation (deepgov.c). The incumbent's empire output
   * for this turn's evaluation burst, stamped by turn AND by the
   * government it was measured under so a stale entry cannot be read
   * as a baseline; gov_valued is the execution census, candidate
   * governments deep priced itself. Recomputable and not saved, the
   * same contract the rates tallies ship under -- the per-candidate
   * deep-gov line is what the benchmark reads. */
  int gov_base_turn;
  int gov_base_gov;
  double gov_base_out;
  int gov_valued;
  /* The revolution's bill: whether EXTRA anarchy turns are real for
   * this player (asked under the incumbent, where the answer lives),
   * the anarchy government's empire output for this burst, and the
   * census of candidates actually charged for one. Above EASY,
   * gov_anarchy_real is FALSE and the charge is the one turn of
   * production the server always takes -- see deepgov.c. */
  bool gov_anarchy_real;
  int gov_anarchy_turn;
  double gov_anarchy_out;
  int gov_anarchy_charged;
  /* The rate ceiling term's model of the empire, taken under the
   * incumbent before any candidate re-arranges the cities, plus the
   * census of candidates whose ceiling actually moved the valuation.
   * Built only when gov_rate_ceiling_gain is on, which is why the
   * validity flag is separate from the turn stamp. */
  bool gov_rate_model_ok;
  struct deep_rate_model gov_rate_model;
  int gov_ceiling_paid;
  /* Diplomacy (PEACE, 2026-08-06). The execution census for the
   * cease-fire pre-pass. Cumulative within a run and NOT saved: the
   * per-event log lines are what benchmark/diplcensus.py reads, and a
   * reload that resets the tallies costs a partial count of an
   * already-logged stream (the same contract the rates counters ship
   * under). */
  /* The runaway-leader read: a per-turn cache stamped by
   * `leaders_turn`, with both consumers going through
   * deep_leaders_refresh() so neither can act on a stale value.
   * Player NUMBERS, -1 for "nobody leads on this axis", which is what
   * a tie means. */
  int leaders_turn;
  int tech_leader_id;
  int prod_leader_id;
  bool wincap_seen;
  int leader_bias_applied;      /* op targets moved by the leader bias */
  /* The never-launches damper (CONSEQ item 5, derby N3). Assault
   * operations FORMED against assault operations that reached MARCH,
   * cumulative and SAVED -- unlike the rates and diplomacy tallies
   * these two drive a decision, so a reload that reset them would
   * change behaviour rather than lose a log line. Derby 3's Iceland
   * built 57 Marines against 6 launches in 600 turns and nothing
   * anywhere connected the two. */
  int ops_formed;
  int ops_launched;
  /* THE UNIT-SINK CENSUS (ERA 3b, CONSEQ N3's launch-rate reality
   * check). Musters entered, musters that reached MARCH, and musters
   * abandoned -- the three numbers that tell a mechanism which masses
   * a decisive force apart from one which parks the army in a yard
   * forever. Cumulative within a run and NOT saved: they are read off
   * the deep-opodds stream, which carries them as running totals, and
   * a reload that resets the tallies costs a partial count of an
   * already-logged stream (the contract the rates and diplomacy
   * counters ship under). Nothing branches on them. */
  int musters_formed;
  int musters_launched;
  int musters_abandoned;
  /* The rotation cadence (NETTHREAT item 3): attack-rated unit-turns
   * spent standing in one of our own cities, over attack-rated
   * unit-turns sampled at all. SAVED, on the same rule the two
   * counters above are saved by -- it drives a build want rather than
   * a log line, so a reload that zeroed it would change behaviour.
   * Zero-defaulted at load, which reads as "no evidence yet" and pays
   * no credit. */
  int cadence_home;
  int cadence_unitturns;
  /* The trade portfolio's two execution counters (TRADE, 2026-08-06):
   * assault targets whose score was actually discounted for being a
   * trade partner's, and love actually added for active routes. Both
   * are zero in every arm whose knob is 0.0, which is what separates
   * "did not execute" from "executed and did nothing". */
  int trade_partner_spared;
  int trade_love_applied;
  int peace_windows;            /* pre-pass evaluations inside a window */
  int peace_proposed;           /* ... that sent a peace clause */
  int peace_taken;              /* peace clauses deep's evaluate accepted */
  /* FENCE. Pacts at peace or above declined because the counterparty
   * is an AI running some other module, and how many of those deep's
   * own valuation says it would otherwise have signed. The second
   * counter is the honesty check: a fence that never intercepts a
   * would-be pact is inert, and an A/B of an inert mechanism is an
   * A/B of noise. */
  int fence_refused;
  int fence_wouldhave;
  int embassy_priced;           /* embassy clauses deep priced itself */
  int embassy_cleared;          /* ... treaties that cleared on deep's sums */
  /* BALANCE. The treaty deep is CONSTRUCTING right now, so that deep's
   * own treaty_evaluate can recognise its own offer and sign it: the
   * clause handler evaluates both sides synchronously inside the
   * suggest call, and stock's valuation of GIVING a tech away is a
   * flat refusal, so without this deep would veto every gift it made.
   * Player NUMBER PLUS ONE, so a zeroed slot means "no offer open" --
   * fc_calloc gives us zeros and player 0 is a real player. Cleared
   * the moment the suggestion returns; never saved, because it cannot
   * outlive one call. */
  int gift_open_plus1;
  int gift_tech;                /* advance in that offer, or A_NONE */
  int gifts_offered;            /* tech gifts proposed */
  int gifts_taken;              /* ... that the counterparty signed */
  int gifts_capped;             /* declined: would jump the alliance bar */
  int gifts_offclass;           /* declined: the relationship's own price */
  int proxies_armed;            /* of those, gifts to a proxy */
  /* The diplomatic state at which we last asked this player for an
   * embassy, PLUS ONE, so a zeroed slot means "never asked". Deep's
   * embassy valuation does not change with the turn number -- that is
   * the bug it replaces -- so a refusal stands until the relationship
   * itself changes. Not saved: the worst case after a reload is one
   * extra ask per pair. */
  signed char embassy_asked[MAX_NUM_PLAYER_SLOTS];
};

static inline struct deep_plr *deep_plr_data(const struct player *pplayer)
{
  return (struct deep_plr *) player_ai_data(pplayer, deep_ai_get_self());
}

bool fc_ai_deep_setup(struct ai_type *ai);

#endif /* FC__DEEPAI_H */
