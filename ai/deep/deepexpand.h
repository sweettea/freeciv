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
#ifndef FC__DEEPEXPAND_H
#define FC__DEEPEXPAND_H

/* The founder funnel: where deep's settler production dies.
 *
 * The war-gap decomposition (STATUS 2026-08-22) rank-3 finding is that
 * on tech50war-200 deep holds 3.7 settlers at T60 against classic's
 * 39.2 (t -9.76) and ends 3.2 cities short. Every stage of the founder
 * chain is already logged SOMEWHERE -- deep-expgate, deep-founder,
 * deep-siteladder, deep-founderbid, deep-bid, deep-want -- and no two
 * of those lines answer the same question, so reading the chain means
 * a six-way join per city-turn across a multi-gigabyte game log.
 *
 * This file is the join, taken where the decisions are made instead of
 * reconstructed afterwards, and reduced to ONE line per player per
 * turn. It follows deepfunnel.c's doctrine exactly: every value here
 * was already computed by the decision it watches, nothing is
 * recomputed, and nothing is read back into a decision.
 *
 * The chain it walks, in the order the code walks it
 * (deepdefence.c deep_manage_cities -> deep_city_choose_build):
 *
 *   cities     city-turns that entered the expansion loop
 *   gate       deep_expansion_blocked() said yes AND the city had a
 *              military want, so founder_want was zeroed and
 *              contemplation skipped entirely
 *   norole     contemplation ran and found no ACTION_FOUND_CITY unit
 *   fresh      contemplation actually ran the site search
 *   stale      the cached want stood (AI_CITY_RECALC_SPEED)
 *   w0/wpos    fresh searches that came home with nothing / with a want
 *   wneg       ... and with a want that needs a boat first
 *   held       city-turns reaching the bid seam with founder_want != 0
 *   vfood      of those, ones where the city's food surplus is below
 *              the founder's upkeep -- daidomestic makes NO founder bid
 *              at all there, at any want
 *   vwonder    the wonder-city divisor applied (/5 or /2)
 *   vcap       the city-count divisor applied (/100)
 *   skipwar    the domestic advisor was never called: war footing and a
 *              military want (dai_city_choose_build's inherited rule)
 *   skipemerg  ... or the military choice sat in the emergency tier
 *   contest    founder_tier_gate: deep's founder want outbid the
 *              military want on magnitude, so the emergency skip was
 *              not taken and the founder was exempt from the tier at
 *              the merge (0 with the gate off)
 *   contwin    ... of those, the ones a founder actually won
 *   hold       founder_run_hold: a founder build already under way was
 *              not switched onto a military unit (0 with the gate off)
 *   holdyld    ... and the ones grave_danger took anyway
 *   auc        city-turns where the domestic auction actually ran with
 *              a founder bid in hand
 *   win        of those, ones the founder won outright
 *   lostdom    the domestic advisor itself preferred worker/building/
 *              caravan
 *   lostmil    the founder won the domestic auction and the military
 *              choice beat it at the merge
 *   lostmilt   ... of those, ones with the military side in the
 *              emergency tier. NOT "lost on tier rather than value":
 *              the domestic bid is clamped to DAI_WANT_DOMESTIC_MAX
 *              (750.075) before it arrives, so it is below any
 *              emergency want under either order (see deepexpand.c)
 *   prod       cities whose production at end of turn IS a founder
 *   born       founders that came into existence this turn
 *   alive      founders alive at census time
 *   found      cities founded this turn
 *   sites      deep_remaining_sites(): how many more cities the
 *              unclaimed land could hold at this player's own packing
 *              density -- the stall-vs-completion discriminator, read
 *              unconditionally so a gated city-turn still reports the
 *              opportunity it declined
 *   freetiles  deep_reachable_free_land()
 *   freeshare  deep_free_land_share()
 *
 * `cities` partitions as gate + norole + fresh + stale by construction,
 * and `auc` as win + lostdom + lostmil, which is the arithmetic
 * self-check on every line.
 */

#include "fc_types.h"

/* deep_expansion_blocked()'s verdict at its call site. `effective` is
 * the verdict AND the second conjunct, i.e. whether founder_want was
 * actually zeroed. */
void deep_expand_note_gate(const struct player *pplayer, bool effective);

/* Contemplation: no founder unit exists in this ruleset/tech state. */
void deep_expand_note_norole(const struct player *pplayer);

/* Contemplation ran (fresh) or the cached want stood (!fresh), and the
 * want that came out of it after deep's own rush and stance terms. */
void deep_expand_note_want(const struct player *pplayer, bool fresh,
                           int want);

/* The bid seam: what ai/default's four transforms did to the want deep
 * produced, and whether the food veto silently removed the bid. */
void deep_expand_note_bid(const struct player *pplayer, int want,
                          bool wonder, bool capped, bool fed);

/* The domestic advisor was skipped for this city-turn. */
void deep_expand_note_skip(const struct player *pplayer, bool war);

/* founder_tier_gate raised a contest: deep's founder want outbid the
 * military want on magnitude, so the emergency skip was not taken and
 * the founder is exempt from the tier at the merge. */
void deep_expand_note_contest(const struct player *pplayer);

/* ... and the contest ended with a founder taking the build. */
void deep_expand_note_contest_win(const struct player *pplayer);

/* founder_run_hold declined a military switch away from a founder
 * build that is already under way. */
void deep_expand_note_hold(const struct player *pplayer);

/* ... and the same run, released because grave_danger was raised. */
void deep_expand_note_hold_yield(const struct player *pplayer);

/* The auction settled. `founder_bid` is the want the advisor compared,
 * `dom_founder` says the domestic advisor's own winner was the founder,
 * `dom_won` the merge verdict, and `mil_emergency` whether the military
 * side carried the emergency tier. */
void deep_expand_note_auction(const struct player *pplayer, int founder_bid,
                              bool dom_founder, bool dom_won,
                              bool mil_emergency);

/* End of this player's city pass: read the tail off the world, print
 * the funnel line, and reset. Called from deep_do_last_activities()
 * after deep_manage_cities(). */
void deep_expand_census(struct player *pplayer);

#endif /* FC__DEEPEXPAND_H */
