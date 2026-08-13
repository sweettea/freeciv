#!/usr/bin/env python3
"""Knob-coverage census: which tunable's mechanism can be SEEN executing.

Campaign v4 gate 7. For every key in `tuning-params.ini` this joins a
hand-built knob -> instrument map (read out of ai/deep, see MAP below)
against the metric lines actually present in a corpus of result dirs,
and reports per knob:

    knob -> instrument -> cells with nonzero firings -> verdict

Verdicts
    LIVE            the instrument fired on >= 1 cell of the corpus
    DEAD            the instrument exists and was readable, and fired
                    nowhere -- the campaign must either exclude the knob
                    or explain the zero
    NO-INSTRUMENT   nothing in ai/deep evidences this knob's arithmetic
                    at LOG_NORMAL; it cannot be censused until one is
                    added (the `need` field says what)
    MODE            a mode switch or degenerate-range key: pinned by
                    hand per pass and reported, never optimised, so it
                    is classified rather than censused

Grades (how much a nonzero count proves)
    direct   the line fires only on the branch this knob gates, or a
             logged field is a monotone function of the knob
    proxy    the line proves the enclosing mechanism ran; the knob's own
             term is not separable from it

Corpus caveats this script enforces rather than assumes:
  * `FREECIV_DEEP_QUIET` demotes every metric line to LOG_VERBOSE
    (deeptuning.c:360). Result dirs run under DEEP_QUIET -- every
    campaign.sh run -- carry NO instrument lines at all. Those dirs are
    reported as `quiet` and excluded from the denominator, not counted
    as zeros.
  * A cell whose logs contain no line with a knob's tag AT ALL is
    reported `n/a` for that knob rather than 0: the binary that wrote
    them may predate the instrument. Only cells that show the tag
    somewhere can vote DEAD.
  * `key.txt` records the server sha and the tuning file. Cells run with
    a non-default tuning file are still evidence that an instrument
    EXISTS; they are weaker evidence of liveness at compiled defaults,
    and are flagged `tuned` per cell.

Usage
    knobcensus.py RESULTS_DIR [RESULTS_DIR ...] [--md] [--seeds N]
                  [--knob NAME] [--dirs]

    --md      emit the markdown table (findings/knob-coverage.md body)
    --seeds N read at most N seed logs per arm (sampling; default all)
    --knob    restrict to one knob (debugging the map)
    --dirs    print the corpus provenance table and exit
"""

import argparse
import collections
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PARAMS = HERE / "tuning-params.ini"

# --------------------------------------------------------------------
# The map. Each entry:
#   knob: (stage, kind, grade, [(tag, predicate), ...], note/need)
#
# kind:  "mag"  a magnitude the optimiser may move
#        "mode" a switch or degenerate range -- classified, not censused
# grade: "direct" | "proxy" | "-" (no instrument)
#
# Predicate language, evaluated against the key=value fields parsed off
# the line (bare words become flags):
#   ""              any line carrying the tag
#   "f>0"/"f>=N"    numeric comparison on field f
#   "f=word"        string equality
#   "f!=word"       string inequality
#   "has:f"         field f present
#   "flag:word"     bare word present on the line
# A knob's probes are OR-ed: any one firing makes it LIVE.
#
# Source lines are ai/deep at the commit this file landed on; the
# consumer sites were enumerated mechanically and the instrument chosen
# by reading the enclosing function.
# --------------------------------------------------------------------

MAP = {
    # ---------------- Stage 3: expansion / site / settlers ----------
    "settler_fearfulness_mult": (
        "S3", "mag", "proxy", [("deep-settler", "")],
        "scales adv_avoid_risks() edge cost inside the settler pf map "
        "(deepsettler.c:213); a deep-settler line proves the risk-aware "
        "map was built, nothing isolates the multiplier. Needs: a "
        "counter of routes whose cost the risk term changed."),
    "settler_risk_tolerance": (
        "S3", "mag", "direct",
        [("deep-settler", "dangerdest>0"), ("deep-settler", "blocker=danger")],
        "deep_civilian_pos_dangerous() veto; dangerdest/blocker=danger "
        "are exactly its refusals (deepsettler.c:177)."),
    "settler_guard_tolerance": (
        "S3", "mag", "-", None,
        "deep_settler_bodyguard_needed() logs only via UNIT_LOG at "
        "LOG_DEBUG (deepsettler.c:331). Needs: a per-turn count of "
        "bodyguard requests raised/cleared on an existing line."),
    "site_enough_result": (
        "S3", "mag", "-", None,
        "the phase-2 skip in deep_find_best_city_placement() "
        "(deepsettler.c:711) is a bare `return cr1`. Needs: a counter "
        "of placements that skipped the overseas phase, ideally as a "
        "field on deep-colony."),
    "founder_pool_per_site": (
        "S3", "mag", "direct", [("deep-pool", "")],
        "deep-pool logs sites= and target= where target is literally "
        "founder_pool_per_site * remaining sites (deepsettler.c:1374)."),
    "site_stream_weight": (
        "S3", "mag", "proxy", [("deep-settler", "flag:pick")],
        "unconditional multiplier inside deep_cityresult_fill(); a pick "
        "line proves the evaluator produced a result. deepsite.c emits "
        "no lines of its own."),
    "site_potential_discount": (
        "S3", "mag", "proxy", [("deep-settler", "flag:pick")],
        "unconditional term of deep_catchment_stream(); same proxy."),
    "site_potential_weight": (
        "S3", "mag", "proxy", [("deep-settler", "flag:pick")],
        "unconditional term of deep_cityresult_fill(); same proxy."),
    "site_travel_discount": (
        "S3", "mag", "proxy", [("deep-settler", "flag:pick")],
        "unconditional relief discount in deep_settler_map_iterate()."),
    "site_settler_cost_weight": (
        "S3", "mag", "proxy", [("deep-settler", "flag:pick")],
        "unconditional cost term in deep_settler_map_iterate()."),
    "site_boat_cost_weight": (
        "S3", "mag", "direct",
        [("deep-settler", "overseas>0"), ("deep-settler", "virt>0")],
        "boat_cost is nonzero only for an overseas/virtual-boat result, "
        "which the pick line reports (deepsite.c:1018)."),
    "site_special_bonus": (
        "S3", "mag", "-", None,
        "nspecials is never logged (deepsite.c:706). Needs: "
        "nspecials/nclaim/contest counts on the deep-settler pick line."),
    "site_special_cluster": (
        "S3", "mag", "-", None,
        "same as site_special_bonus, and additionally needs "
        "nspecials >= 2 to be nonzero at all."),
    "site_claim_bonus": (
        "S3", "mag", "-", None,
        "nclaim is never logged (deepsite.c:709). Same need."),
    "site_pressure_factor": (
        "S3", "mag", "-", None,
        "the `pressured` predicate is never logged (deepsite.c:667). "
        "Needs: a pressured-catchment count on the pick line."),
    "site_travel_crowding": (
        "S3", "mag", "-", None,
        "crowding relief is folded into the discount and never logged "
        "(deepsite.c:1006). Default 0.0 makes the term identity."),
    "site_rival_pressure": (
        "S3", "mag", "-", None,
        "the `contest` count is never logged (deepsite.c:765). Needs a "
        "contest field on the pick line; venue-sensitive by design."),
    "site_rival_crowding": (
        "S3", "mag", "-", None,
        "shapes the same unlogged contest penalty (deepsite.c:766)."),
    "site_choke_bonus": (
        "S3", "mag", "-", None,
        "the choke predicate is never logged (deepsite.c:769)."),
    "settler_rush_weight": (
        "S3", "mag", "-", None,
        "multiplies founder_want in deep_manage_cities() AFTER "
        "deep-def is written (deepdefence.c:3868), so the founder= "
        "field on deep-def is the pre-rush number. Needs: the post-rush "
        "want logged, or a rush-applied counter."),
    "expansion_war_survival": (
        "S3", "mag", "direct", [("deep-expgate", "wave>=0")],
        "deep-expgate is the blocked-expansion counter this entry asked "
        "for (ERA, 2026-08-11): one line per deep_expansion_blocked() "
        "evaluation carrying surv, thresh and blocked. wave>=0 is the "
        "knob's own branch -- the gate logs wave=-1 on the two paths "
        "that never reach the survival comparison, which is exactly "
        "where the knob is <= 0 or the empire is at peace."),
    "expansion_gate_enabled": (
        "S3", "mode", "direct", [("deep-expgate", "wave=-2")],
        "the never-block arm of the gate (deepdefence.c, "
        "deep_expansion_blocked()). Degenerate range: pinned per "
        "experiment, never optimised. wave=-2 is its own branch -- the "
        "gate logs it only when the knob is 0, and at the shipped "
        "default 1 it never appears, so MODE with an empty live list is "
        "the expected reading at defaults."),
    "site_wait_discount": (
        "S3", "mag", "direct", [("deep-sitewait", "")],
        "W36 expected-wait coupling (deep_settler_map_iterate overseas "
        "leg + the colony probe by construction): each overseas "
        "candidate discounted by the tail of its water's live lift "
        "wait spells. deep-sitewait logs per-search candidate counts "
        "and wait-turns (liveness: 1.92 turns/candidate mean on "
        "isolated-150). 0 = off; A/B queued."),
    "lift_wait_tail_pct": (
        "S3", "mode", "direct", [("deep-waitspell", "")],
        "which percentile of the live spell distribution "
        "deep_wait_expected() reports. 80 = the measured tail boundary "
        "(spells >=5 turns are 19.8% of spells, 61.4% of waiting; "
        "commission-sizing.md). Sweep with site_wait_discount, never "
        "alone."),
    "op_draft_marginal": (
        "S3", "mode", "direct", [("deep-opodds", "drafted>=1")],
        "ERA 3a's marginal garrison release (deepops.c, deep_op_form). "
        "0/1 switch; the drafted field of deep-opodds is the observing "
        "counter (arm A' 0.18 vs arm A 0.30, 2026-08-12). Falsified "
        "and score-null on tech50war-200 (STATUS 2026-08-12); stays 0 "
        "pending the MUSTER redesign."),
    "op_muster": (
        "S3", "mode", "direct", [("deep-opodds", "muster>=1")],
        "ERA 3b's muster (deepops.c, deep_op_form + deep_op_drive): a "
        "slot whose target clears the worth gate enters MUSTER with "
        "the odds refused, accumulates across turns and launches when "
        "the odds at the current pool clear the bar. 0/1 switch. The "
        "muster field of deep-opodds is 1 only on lines a muster "
        "produced (verdict muster|muster-hold|muster-launch|"
        "muster-abandon); the pre-3b population is verdict "
        "formed|refused, where it is 0."),
    "op_muster_stall": (
        "S3", "mode", "direct", [("deep-opodds", "why=muster-stall")],
        "turns without a new high-water member count before ERA 3b "
        "abandons a muster. PLACEHOLDER (no accumulation has ever been "
        "observed, so there is no cadence to read it off). The stall "
        "field counts the clock on every muster line; why=muster-stall "
        "is the abandon it causes, beside muster-worth and "
        "muster-home."),
    "op_muster_home": (
        "S3", "mode", "direct", [("deep-opodds", "why=muster-home")],
        "ERA 3b's home-defence abandon trigger, separably. Measured on "
        "tech50war-200 it is not one of three triggers but THE trigger: "
        "155 of 191 abandons (81%) against 34 worth and 2 stall, with "
        "only 5 of 433 musters ever launching. 1 = armed (as designed); "
        "0 = the stall clock and worth gate only. Its own arm."),
    "survival_pf_reach": (
        "S3", "mode", "direct", [("deep-wave", "pf_turns>=0")],
        "ERA 3c's pathfinder-confirmed land reach "
        "(deepcombat.c, deep_city_survival_core). 0/1 switch; "
        "deep-wave's far_dist/far_turns/pf_turns fields are its "
        "instrument, and the nonzero-wave row count is its footprint "
        "(45,034 -> 31,802 when on). Factorial arm, 2026-08-12."),
    "attacker_garrison_gain": (
        "S3", "mag", "direct", [("deep-agc", "")],
        "NETTHREAT item 3 (deepunit want seam): credit an attacker's "
        "build want with marginal attrition x menace x OBSERVED home "
        "cadence (deep-cadence, ~0.89 mean). deep-agc lines appear "
        "only when the gain is nonzero; 0 lines at default is the "
        "expected reading (off-switch proof 2026-08-12)."),
    "cover_removal": (
        "S3", "mode", "direct", [("deep-cover", "")],
        "NETTHREAT item 4 (deep_cover_removal_cost): price defender "
        "removal after the cheapest covering move; consumers are the "
        "sortie gate and both draft release tests. deep-cover lines "
        "only at knob 1; factorial-sensitive (moves the 3a cost side), "
        "enters that design as its own arm."),
    "worker_flee_radius": (
        "S3", "mag", "direct", [("deep-flee", "")],
        "deep-flee is emitted exactly when a worker flees "
        "(deepdefence.c:1635)."),
    "worker_guard_radius": (
        "S3", "mag", "direct", [("deep-guard", "")],
        "deep-guard is the guard branch of deep_flee_workers() "
        "(deepdefence.c:1609). It does NOT cover the second consumer, "
        "the deep_worker_task_value() discount."),
    "worker_danger_discount": (
        "S3", "mag", "-", None,
        "deep_worker_task_value() divides the want and logs nothing "
        "(deepsettler.c:1873). Needs: a discounted-task counter, or the "
        "pre/post want on deep-idlework."),
    "worker_cross_continent_discount": (
        "S3", "mag", "direct", [("deep-idlework", "cross>0")],
        "the cross=1 form of deep-idlework is emitted only on the "
        "cross-water fallback the discount prices "
        "(deepsettler.c:1194/1257)."),
    "worker_travel_discount": (
        "S3", "mag", "direct", [("deep-worklocal", "disc>0")],
        "disc= counts primary-auction candidates deep_worker_travel_"
        "price() actually discounted, which requires "
        "worker_travel_discount > 0.0 and arrival > 0 "
        "(deepsettler.c:2189). cand= counts every candidate offered "
        "and fires at any knob value, so it does not evidence it; "
        "meanwalk= is the behaviour metric, comparable across a "
        "knobs-on and knobs-off run of the same seed."),
    "trunk_road_gain": (
        "S3", "mag", "direct", [("deep-trunk", "hits>0")],
        "deep_trunk_want() returns before building a plan when "
        "trunk_road_gain <= 0.0, so the deep-trunk line does not "
        "exist at all below the gate (deeptrunk.c:283); hits= counts "
        "the road candidates the want actually reached, and "
        "maxwant= is a monotone multiple of the gain. links>0 with "
        "hits=0 is the radius ceiling, not a dead knob."),

    # ---------------- Stage 1: economy / horizon / CM ---------------
    "horizon_capacity_weight": (
        "S1", "mag", "direct", [("deep-capacity", "credit>0")],
        "deep-capacity logs the credit this weight scales, one line per "
        "capped city per turn (deephorizon.c:263)."),
    "horizon_growth_weight": (
        "S1", "mag", "proxy", [("deep-credit", "")],
        "deep_growth_credit() has no line of its own; only the "
        "per-city aggregate deep-credit and the LOG_DEBUG deep-horizon "
        "see it. Needs: a per-term breakdown on deep-credit."),
    "horizon_bundle_weight": (
        "S1", "mag", "proxy", [("deep-credit", "")],
        "as horizon_growth_weight; additionally conditional on the "
        "ruleset having bundle effects."),
    "horizon_content_weight": (
        "S1", "mag", "proxy", [("deep-credit", "")],
        "as horizon_growth_weight; conditional on a pacification load."),
    "horizon_multiplier_weight": (
        "S1", "mag", "proxy", [("deep-credit", "")],
        "as horizon_growth_weight; conditional on multiplier effects."),
    "horizon_health_weight": (
        "S1", "mag", "-", None,
        "gated on game.info.illness_on AND size >= illness_min_size, "
        "and folded into the same unlogged `add` "
        "(deephorizon.c:887). Needs: a health-credit counter; "
        "ruleset-locked, so pin the venue too."),
    "horizon_tech_weight": (
        "S1", "mag", "-", None,
        "deep-techwall is written at LOG_DEBUG (deephorizon.c:850), so "
        "it is absent from every benchmark log. Needs: promote it to "
        "deep_metric_log_level() or add a techwall counter to "
        "deep-credit. Standing receipt: 0 executions in 70 games."),
    "upkeep_gold_weight": (
        "S1", "mag", "proxy", [("deep-credit", "")],
        "subtracted from the same aggregate `add` (deephorizon.c:926). "
        "Needs: the charged upkeep on deep-credit."),
    "cm_happy_factor": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_war_food": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_war_shield": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_war_trade": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_cap_food": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_cap_shield": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_cap_trade": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_wonder_shield": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_wonder_trade": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_grow_below": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_grow_safety_margin": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_grow_food": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_grow_shield": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_grow_trade": ("S1", "mag", "-", None, "CM parameters: deepcm.c emits no log line at all."),
    "cm_founder_food": (
        "S1", "mag", "direct",
        [("deep-cmfound", "floor>=1")],
        "the founder food floor (deepcm.c). The line is written only on "
        "the branch the knob gates, and `floor` is ceil(knob x upkeep), "
        "so a nonzero count is the knob's own arithmetic executing."),
    "cm_trade_quality_slope": (
        "S1", "mag", "-", None,
        "CM trade-quality reweight (deepcm.c:284); no line, and the "
        "`matched` predicate is itself unobservable."),
    "cm_war_survival": (
        "S1", "mag", "-", None,
        "the war-CM threshold (deepcm.c:219). deep-surv proves the "
        "survival estimator runs, but NOT that the war branch of "
        "deep_set_city_manager_params() was taken."),
    "sortie_defence_weight": (
        "S1", "mag", "-", None,
        "deep_cm_sortie_value() (deepcm.c:131); no line. Needs: the "
        "sortie value on an existing CM or defence line."),
    "embassy_science_value": (
        "S1", "mag", "-", None,
        "diplomat want bump in deep_military_choose_build() "
        "(deepdefence.c:2717); no line. Needs: a diplomat-want counter, "
        "or route it through deep-want."),
    "explore_hut_value": (
        "S1", "mag", "direct", [("deep-explore", "huts_vis>0")],
        "deep-explore logs huts_vis, the multiplicand of this weight "
        "(deepexplore.c:186)."),
    "explore_discovery_value": (
        "S1", "mag", "direct", [("deep-explore", "unknown>0")],
        "deep-explore logs unknown, the multiplicand of this weight. "
        "STRUCTURALLY ZERO: deepexplore.c:180 sets unknown = 0 whenever "
        "the player lacks the H_MAP handicap, which is every difficulty "
        "actually played, so the discovery term is disabled pending W04 "
        "and the knob multiplies nothing."),
    "hut_risk_radius": (
        "S1", "mag", "direct", [("deep-explore", "huts_disc>=1")],
        "huts_disc= is the count of huts the radius form discounted, "
        "logged on every deep-explore line (deepexplore.c). Nonzero "
        "only while barbs_armed=1 (the server's rate+onset gate) and "
        "hut_risk_honest=0, which is the shipped default."),
    "hut_risk_discount": (
        "S1", "mag", "direct", [("deep-explore", "huts_disc>=1")],
        "as hut_risk_radius: huts_disc= counts exactly the huts this "
        "weight is applied to, under whichever form is selected."),
    "hut_risk_honest": (
        "S1", "gate", "direct", [("deep-explore", "huts_unleash>=1")],
        "the sign correction: charge hut_risk_discount against the "
        "huts the SERVER would let unleash (no city of any player "
        "within CITY_MAP_MAX_RADIUS, default.lua:112) instead of the "
        "huts near an own city. huts_unleash= counts that population "
        "on every deep-explore line whether the knob is on or off, so "
        "the flip's live surface is measured at the compiled default "
        "of 0; huts_disc= then reports which set was actually charged."),
    "rates_science_gain": (
        "S1", "mag", "direct", [("deep-rates", "vsci>0")],
        "deep-rates logs vsci, the science valuation this gain scales "
        "(deeprates.c:410). Inert while rates_enabled = 0."),
    "rates_luxury_gain": (
        "S1", "mag", "direct", [("deep-rates", "vlux>0")],
        "deep-rates logs vlux (deeprates.c:418). Inert while "
        "rates_enabled = 0."),
    "gov_anarchy_weight": (
        "S1", "mag", "direct", [("deep-gov", "ancost>0")],
        "deep-gov logs ancost, the revolution charge this weight "
        "scales (deepgov.c). Inert while gov_valuation = 0, and zero "
        "above skill EASY, where the server charges an AI no anarchy "
        "(H_REVOLUTION unset)."),
    "gov_rate_ceiling_gain": (
        "S1", "mag", "direct", [("deep-gov", "ceil>0")],
        "deep-gov logs ceil, the rate-ceiling credit this gain scales "
        "(deepgov.c). Default 0.0 = off: expect ceil=0.0 on every line "
        "even in a gov_valuation = 1 arm."),

    # ---------------- Stage 2: military ------------------------------
    "committed_fear_mult": (
        "S2", "mag", "-", None,
        "multiplies the survival-deficit fear only when "
        "deep_committed_away() (deepdefence.c:873); deep-surv logs the "
        "resulting fear but not the away predicate. Needs: away= on "
        "deep-surv."),
    "incoming_defence_eta_weight": (
        "S2", "mag", "-", None,
        "the en-route credit is subtracted from danger without a line "
        "(deepdefence.c:212/981). Needs: credit= on deep-def, or a "
        "second deep-def line after the injection pass."),
    "incoming_defence_survival": (
        "S2", "mag", "-", None, "as incoming_defence_eta_weight."),
    "overseas_fear": (
        "S2", "mag", "-", None,
        "injected into city_data->danger AFTER deep-def is written "
        "(deepdefence.c:971), so the logged danger is stock's raw "
        "figure. Needs: a post-injection danger line, or per-fear "
        "fields (over/shadow/density/barb) on deep-def."),
    "overseas_decay": ("S2", "mag", "-", None, "as overseas_fear; same line, same need."),
    "boat_shadow_fear": ("S2", "mag", "-", None, "as overseas_fear; same line, same need."),
    "density_fear_value": ("S2", "mag", "-", None, "as overseas_fear; same line, same need."),
    "density_fear_radius": ("S2", "mag", "-", None, "as overseas_fear; same line, same need."),
    "density_fear_ramp_turns": (
        "S2", "mag", "-", None,
        "the anticipation ramp scales the density prior, unlogged "
        "(deepdefence.c:605). Default 0 disables the ramp outright."),
    "density_prior_value": ("S2", "mag", "-", None, "as overseas_fear; same line, same need."),
    "density_prior_tiles": (
        "S2", "mag", "-", None,
        "map-crowding denominator (deepdefence.c:552), consumed by the "
        "density prior and remaining-sites estimate; neither is logged."),
    "land_pressure_tiles": (
        "S2", "mag", "-", None,
        "deep_land_pressure() (deepdefence.c:587) feeds the same "
        "unlogged prior."),
    "barbarian_fear": ("S2", "mag", "-", None, "as overseas_fear; same line, same need."),
    "wall_fear_weight": (
        "S2", "mag", "-", None,
        "adds to the wall's building want in "
        "deep_military_choose_build() (deepdefence.c:2526) with no "
        "line. Needs: a wall-want counter, or route it via deep-want."),
    "garrison_floor": (
        "S2", "mag", "proxy",
        [("deep-sat", ""), ("deep-rotate", "")],
        "the floor is read by the satisfaction gate and by the melt "
        "count; both emit lines, neither isolates the floor value "
        "(deepdefence.c:837/914/1051)."),
    "garrison_floor_danger": (
        "S2", "mag", "-", None,
        "the floor's injected danger is added after deep-def "
        "(deepdefence.c:839). Needs: the same post-injection line the "
        "fear lattice needs."),
    "garrison_menace_slope": (
        "S2", "mag", "direct", [("deep-def", "menace>0")],
        "deep-def carries menace=, which is deep_garrison_menace()'s "
        "return (deepdefence.c:814)."),
    "menace_overseas_weight": (
        "S2", "mag", "proxy", [("deep-def", "menace>0")],
        "the same menace figure, but this weight only bites on the "
        "unreachable fraction and on the density/barbarian terms; a "
        "nonzero menace does not prove it applied."),
    "survival_target": (
        "S2", "mag", "direct", [("deep-surv", ""), ("deep-meltblock", "")],
        "deep_survival_deficit_fear() returns 0 before logging when "
        "survival_target <= 0, so a deep-surv line proves the threshold "
        "is live (deepdefence.c:863)."),
    "survival_fear_value": (
        "S2", "mag", "direct", [("deep-surv", "fear>0")],
        "fear= is this value scaled by the deficit "
        "(deepdefence.c:876)."),
    "survival_horizon": (
        "S2", "mag", "direct", [("deep-surv", "")],
        "the wave= on deep-surv is scanned over exactly this horizon."),
    "satisfied_fear_discount": (
        "S2", "mag", "direct", [("deep-sat", "")],
        "deep-sat is emitted exactly when the satisfaction gate "
        "returned < 1.0 (deepdefence.c:963)."),
    "upkeep_unit_weight": (
        "S2", "mag", "-", None,
        "deep_defender_upkeep_discount() (deepdefence.c:410) scales the "
        "fear injection and the rotation want, unlogged. Needs: updisc= "
        "on deep-def."),
    "rotation_power_ratio": (
        "S2", "mag", "direct", [("deep-rotate", "")],
        "deep_melt_surplus_defenders() returns before logging when the "
        "ratio is <= 0; deep-rotate is its only outcome "
        "(deepdefence.c:1005/1135/1177)."),
    "rotation_want": (
        "S2", "mag", "-", None,
        "the quiet-city rotation BID in deep_military_choose_build() "
        "(deepdefence.c:2405) is not logged; deep-rotate covers the "
        "melt/upgrade side only. Needs: a rotation-bid counter or a "
        "deep-want tier."),
    "rotation_finish_relax": (
        "S2", "mag", "proxy", [("deep-rotate", "has:disband")],
        "relaxes the melt's obsolescence bar (deepdefence.c:1099); a "
        "disband line proves the bar was cleared, not that the relax "
        "was what cleared it."),
    "tired_attack_floor": (
        "S2", "mag", "direct", [("deep-tired", "")],
        "deep-tired is emitted exactly when a tired attacker is held "
        "(deepdefence.c:1513). Ruleset-gated on game.info.tired_attack: "
        "venue-locked."),
    "attack_value_weight": (
        "S2", "mag", "direct", [("deep-kill", "")],
        "deep-kill logs the assault want this weight scales "
        "(deepdefence.c:3177/3247)."),
    "attack_loss_gain": (
        "S2", "mag", "direct", [("deep-kill", "")],
        "deep-kill carries gross= and loss=, so the cost-of-victory "
        "subtraction this gain scales is on the line whether or not it "
        "changed the argmax (deepdefence.c:3813)."),
    "conquest_worth_mult": (
        "S2", "mag", "direct", [("deep-kill", ""), ("deep-netval", "")],
        "both conquest lines carry worth= (stock's read of the city) "
        "and eworth= (after the multiplier), so the ratio IS the knob "
        "and it is on every evaluation win or lose."),
    "defence_worth_mult": (
        "S2", "mag", "direct", [("deep-op", "flag:poach")],
        "the poach line carries worth= and dworth= (after the "
        "multiplier) beside the risk it produced; MAP -- the knob is "
        "one half of the offence/defence TILT and only the RATIO to "
        "conquest_worth_mult is meaningful, so a sweep must move them "
        "together or explicitly hold one."),
    "assault_pool_reach": (
        "S2", "gate", "direct", [("deep-kill", "")],
        "far= counts the free attackers the per-target pool dropped "
        "for being unable to arrive by the marginal blade's eta; 0 on "
        "every line is the off arm (deepdefence.c:3720)."),
    "op_launch_odds": (
        "S2", "gate", "direct", [("deep-op", "flag:march")],
        "the march line carries bar=, odds= and surv=: bar=0 is the "
        "head-count launch, bar=1 the odds one (deepops.c:3760)."),
    "op_net_value": (
        "S2", "gate", "direct", [("deep-netval", "flag:refuse")],
        "deep-netval is emitted exactly when the total-form value test "
        "refuses a launch the odds bar would have taken "
        "(deepops.c:3860); zero lines is the off arm."),
    "op_hold_survivors": (
        "S2", "mag", "direct", [("deep-op", "flag:march")],
        "surv= on the march line is the expected force still standing "
        "after the take, which this bar is compared against; a launch "
        "that never happens leaves no line, so pair it with op_form."),
    "sortie_win_floor": (
        "S2", "mag", "direct", [("deep-sortie", "has:kill")],
        "the kill= form of deep-sortie fires only after the duel "
        "cleared the floor (deepdefence.c:1355)."),
    "op_take_target": (
        "S2", "mag", "direct",
        [("deep-kill", ""), ("deep-op", "flag:retarget")],
        "the take bar prices deep_assault_want() and gates op forming "
        "and retargeting (deepdefence.c:3141, deepops.c:2706/3059)."),
    "op_muster_wait": (
        "S2", "mag", "direct",
        [("deep-op", "flag:abort-wait"), ("deep-op", "flag:abort-march"),
         ("deep-op", "flag:strike"), ("deep-op", "flag:garrison")],
        "abort-wait / abort-march are the muster timeout firing "
        "(deepops.c:3596/3712); strike and garrison are its patience "
        "expiring usefully."),
    "op_wave_window": (
        "S2", "mag", "direct", [("deep-op", "wave2>0")],
        "the form line carries wave2=, set exactly when the previous "
        "strike is inside the window (deepops.c:2649/2849)."),
    "op_abort_floor": (
        "S2", "mag", "direct",
        [("deep-op", "flag:abort-strike"), ("deep-op", "flag:abort-odds")],
        "both dissolutions are the floor firing "
        "(deepops.c:3384/3672)."),
    "raid_radius": (
        "S2", "mag", "direct", [("deep-raid", ""), ("deep-op", "flag:raid")],
        "deep_op_form_raid() returns before forming when the radius is "
        "<= 0 (deepops.c:2451); deep-raid is the drive "
        "(deepops.c:2518)."),
    "invasion_reinforce_rate": (
        "S2", "mag", "direct",
        [("deep-invade", ""), ("deep-kill", "eta>=1")],
        "deep-invade's p= is deep_take_odds_at_arrival(), whose "
        "reinforcement model this rate is (deepcombat.c:438). The "
        "second probe is the BUILD side of the same law: every "
        "deep-kill line with eta>=1 priced its odds against a garrison "
        "this rate reinforced, and it is orders of magnitude more "
        "populous than the crossing seam, which needs an operation "
        "actually mid-sail. sea=1 on that line separates the invasion "
        "wants from the land ones."),
    "barb_reinforce_rate": (
        "S2", "mag", "proxy", [("deep-kill", "barb>0")],
        "the barbarian branch of deep_take_odds_at_arrival() "
        "(deepcombat.c:437). Default 0.0 makes it an identity: a "
        "nonzero count proves barbarian targets are priced, not that "
        "the rate moved anything."),
    "diplomat_defence_value": (
        "S2", "mag", "-", None,
        "the diplomat's defensive value in "
        "deep_military_choose_build() (deepdefence.c:2422); no line."),
    "convoy_min_load": (
        "S2", "mag", "direct", [("deep-convoy", "flag:hold")],
        "the hold form of deep-convoy is the min-load wait firing "
        "(deepdefence.c:1248/1274)."),
    "convoy_wait_radius": (
        "S2", "mag", "proxy", [("deep-convoy", "flag:hold")],
        "the radius is tested inside the same hold decision "
        "(deepdefence.c:1266); the line does not report the distance."),

    # ---------------- Stage 3: the lift cluster ----------------------
    "colony_lift_weight": (
        "S3", "mag", "direct", [("deep-colony", "has:lift")],
        "the lift= form of deep-colony is the colony-lift want being "
        "raised (deepdefence.c:2938). The demand= form is the settler "
        "side's probe and does NOT prove this weight applied."),
    "lift_muster_release": (
        "S3", "mode", "direct", [("deep-lift-invalid", "phase=muster")],
        "the muster-phase half of D-E's mission re-validation "
        "(deepops.c, deep_op_drive_lift). 0/1 switch. The muster form "
        "of deep-lift-invalid is written in BOTH arms -- counting at "
        "0, releasing at 1 -- so it proves the population exists, not "
        "that the release ran; the op's `lift-invalid` dissolution and "
        "`reaim` are what separate the arms."),
    "founder_berth_route": (
        "S3", "mode", "direct", [("deep-berthroute", "dist>=1")],
        "EXPANSION slice 3's demand-side route (deepsettler.c, "
        "deep_settler_berth_route). 0/1 switch. deep-berthroute is "
        "written ONLY when the knob acts, and dist is the walk it "
        "ordered, so the line cannot fire in the off arm; the "
        "population it acts on is deep-berth coast=1 serves=0, which "
        "is counted in both."),
    "hull_commission_weight": (
        "S3", "mag", "direct", [("deep-commission", "gap>=1")],
        "EXPANSION slice 2's commission (deepdefence.c, the colony "
        "lift block): overseas founder demand beyond the berths idle "
        "and building on one water, priced as the settlements the gap "
        "holds times the delay a hull removes. deep-commission is "
        "written only on the branch the knob gates and carries the gap "
        "and the want, so a nonzero count is this arithmetic running. "
        "Population is small by measurement -- 7 positive gaps on "
        "bridging water in 600 water-turns of isolated-150 "
        "(deep-liftgap) -- so read `beat` before reading firings."),
    "boat_idle_damping": (
        "S3", "mag", "direct", [("deep-hull", "")],
        "deep-hull logs want before -> after the idle damping "
        "(deepdefence.c:3921)."),
    "lift_form_radius": (
        "S3", "mag", "direct", [("deep-lift-op", "flag:form")],
        "a lift op only forms when a passenger is inside the radius "
        "(deepops.c:1453/1528)."),
    "lift_gather_turns": (
        "S3", "mag", "direct",
        [("deep-op", "flag:lift-gather-timeout"),
         ("deep-lift-op", "flag:hitch-wait")],
        "the timeout dissolution and the hitch-wait line are both "
        "budgeted by this (deepops.c:1683/1723)."),
    "lift_cross_turns": (
        "S3", "mag", "direct", [("deep-op", "flag:lift-cross-timeout")],
        "the crossing timeout dissolution (deepops.c:1985)."),
    "lift_op_slots": (
        "S3", "mag", "proxy", [("deep-hitch", "")],
        "deep-hitch reports liftslots= and opfull=; opfull=1 is the "
        "population where the slot count actually binds "
        "(deepops.c:823/3914)."),
    "lift_manifest_max": (
        "S3", "mag", "proxy", [("deep-lift-op", "flag:hitch")],
        "the cap on deep_lift_enroll()'s room (deepops.c:1281); the "
        "hitch line reports manifest= but not whether the cap bound."),
    "lift_hitch_wait_ratio": (
        "S3", "mag", "direct", [("deep-lift-op", "flag:hitch-wait")],
        "deep_lift_worth_waiting() returns FALSE when the ratio is <= 0, "
        "so a hitch-wait line proves it (deepops.c:1358)."),
    "lift_hitch_wait_min": (
        "S3", "mag", "proxy", [("deep-lift-op", "flag:hitch-wait")],
        "the floor under the same budget (deepops.c:1369); it binds "
        "only when ratio * eta_cross < min, which is not logged."),
    "lift_drop_radius": (
        "S3", "mag", "direct", [("deep-lift-op", "flag:nextstop")],
        "the nextstop line is emitted exactly when a further drop is "
        "inside the detour radius (deepops.c:1600/1607)."),

    # ---------------- Stage 4: trade ---------------------------------
    "caravan_discount": (
        "S4", "mag", "direct", [("deep-trade", "has:discount")],
        "deep-trade prints the discount it ran with "
        "(deeptrade.c:232/278)."),
    "trade_merit_gain": (
        "S4", "mag", "direct", [("deep-trade-merit", "merit>0")],
        "merit= is the NPV converted by this gain (deeptrade.c:499)."),
    "wonder_merit_gain": (
        "S4", "mag", "direct", [("deep-wonder-merit", "merit>0")],
        "merit= is the wonder want, scaled by the share of the wonder "
        "the caravan's shields buy and amortized over build+walk, "
        "converted by this gain (deeptrade.c)."),

    # ---------------- Mode switches and degenerate ranges ------------
    "caravan_foreign_trade": (
        "S4", "mode", "direct", [("deep-trade", "has:reach")],
        "an ENUM selecting foreign_trade_limit (deeptrade.c:108), not a "
        "magnitude: 0/1/2/3 are four different candidate sets. Pin per "
        "pass and report reach=, do not hand it to the optimiser as a "
        "continuous dimension."),
    "trade_merit": (
        "S4", "mode", "direct", [("deep-trade-merit", "")],
        "off-switch for the whole merit bid (deeptrade.c:582). Default "
        "0: expect no deep-trade-merit lines at defaults."),
    "wonder_merit": (
        "S4", "mode", "direct", [("deep-wonder-merit", "")],
        "off-switch for the wonder-caravan merit bid (deeptrade.c). "
        "Default 0: expect no deep-wonder-merit lines at defaults."),
    "pay_switch_penalty": (
        "S4", "mode", "direct", [("deep-payswitch", "")],
        "honesty switch, not a magnitude: deep issues itself "
        "H_PRODCHGPEN so the change-production penalty applies to its "
        "own seats (deepai.c). One deep-payswitch line per seat per "
        "game when on; deep-prodswitch pen=1 says the loss on that "
        "event was really taken. Default 0: expect neither at "
        "defaults."),
    "switch_cost_price": (
        "S4", "mode", "direct",
        [("deep-switchcost", ""), ("deep-switchhold", "")],
        "off-switch for pricing the switch in the build auction "
        "(deepdefence.c). Default 1: expect deep-switchcost lines at "
        "defaults, and deep-switchflip when the debit changed the "
        "winner."),
    "switch_cost_gain": (
        "S4", "mag", "direct", [("deep-switchcost", "debit>0")],
        "debit= is the real forfeit converted at SHIELD_WEIGHTING and "
        "then by this gain (deepdefence.c)."),
    "scale_settler": (
        "S4", "mag", "direct", [("deep-scale", "pick=founder")],
        "the per-class scale on ai/default's founder_want, applied "
        "before domestic_advisor_choose_build() runs (deepdefence.c). "
        "Default 1.0 is skipped, not multiplied: expect NO deep-scale "
        "lines at compiled defaults."),
    "scale_worker": (
        "S4", "mag", "direct", [("deep-scale", "pick=worker")],
        "the same, on worker_want (deepdefence.c). Default 1.0: no "
        "line at defaults."),
    "scale_defender": (
        "S4", "mag", "direct", [("deep-scale", "class=1")],
        "the per-class scale on deep's military choice when it is a "
        "CT_DEFENDER, NORMAL tier only (deepwant.c). Default 1.0: no "
        "line at defaults."),
    "scale_attacker": (
        "S4", "mag", "direct", [("deep-scale", "class=2")],
        "the same for CT_ATTACKER without transport capacity "
        "(deepwant.c). Default 1.0: no line at defaults."),
    "scale_ferry": (
        "S4", "mag", "direct", [("deep-scale", "class=3")],
        "the same for a transport-capable hull -- the attacker ferry "
        "and the colony lift, which file as CT_ATTACKER (deepwant.c). "
        "Default 1.0: no line at defaults."),
    "emergency_value_cap": (
        "S4", "mag", "direct", [("deep-scale", "scale=CAP")],
        "caps the VALUE inside the emergency tier, never the tier "
        "(deepwant.c). Default 0 = no cap: expect no scale=CAP lines "
        "at defaults."),
    "boat_water_gate": (
        "S3", "mode", "direct", [("deep-watergate", "")],
        "A/B off-switch for the water gate (deepdefence.c:3494)."),
    "boat_gate_at_bid": (
        "S3", "mode", "direct",
        [("deep-watergate-bid", ""), ("deep-watergate-attack", "")],
        "A/B switch moving the gate from veto to bid time "
        "(deepdefence.c:2783/3546)."),
    "rates_enabled": (
        "S1", "mode", "direct", [("deep-rates", "")],
        "off-switch for the whole rates model (deeprates.c:463). "
        "Default 0: expect no deep-rates lines at defaults."),
    "gov_valuation": (
        "S1", "mode", "direct", [("deep-gov", "")],
        "off-switch for deep's whole government valuation "
        "(deepgov.c). Default 0 forwards the gov_value hook to stock: "
        "expect no deep-gov lines at defaults."),
    "stance_forced": (
        "S1", "mode", "direct", [("deep-stance", "mode=PINNED")],
        "degenerate range -1,-1: the pinned corner selector. A pass "
        "pins it by hand and reports mode=."),
    "stance_source": (
        "S1", "mode", "direct", [("deep-stance", "has:src")],
        "degenerate range 0,0: which source seeds the axes."),
    "stance_trait_span": (
        "S1", "mode", "direct", [("deep-stance", "has:src")],
        "degenerate range 0.5,0.5: the depth of the pin, part of the "
        "pin rather than a magnitude until E34.3 widens it."),
    "stance_fear_gain": (
        "S2", "mag", "direct", [("deep-stance", "fear_gate>0")],
        "gate-1 counter, cumulative on deep-stance. Increments only "
        "when the multiplier != 1.0, i.e. never under FREE identity "
        "axes (deepdefence.c:968) -- pin-conditional by construction."),
    "stance_op_gain": (
        "S2", "mag", "direct", [("deep-stance", "op_gate>0")],
        "gate-2 counter (deepops.c:2712); pin-conditional."),
    "stance_growth_gain": (
        "S1", "mag", "direct", [("deep-stance", "growth_gate>0")],
        "gate-3 counter (deephorizon.c:902); pin-conditional."),
    "stance_expand_gain": (
        "S1", "mag", "direct", [("deep-stance", "expand_gate>0")],
        "gate-4 counter (deepdefence.c:2873/3885); pin-conditional."),
    "stance_assault_gain": (
        "S2", "mag", "direct", [("deep-stance", "assault_gate>0")],
        "gate-5 counter (deepdefence.c:3242); pin-conditional."),
    "stance_floor_gain": (
        "S2", "mag", "direct", [("deep-stance", "floor_gate>0")],
        "floor_gate= increments in deep_apply_garrison_floor() only "
        "when the multiplier != 1.0 (deepdefence.c:1750-1751), which "
        "cannot happen at stance_floor_gain's identity value."),
    "stance_position_gain": (
        "S1", "mag", "proxy", [("deep-stance", "pos_moved>0")],
        "pos_moved counts turns either this gain or "
        "stance_techlean_gain moved an axis off the trait prior "
        "(deepstance.c:454-456); not separable between the two, and "
        "zeroed outright unless stance_source resolves to 2 (source "
        "1/traits never reads position at all)."),
    "stance_techlean_gain": (
        "S1", "mag", "proxy", [("deep-stance", "pos_moved>0")],
        "as stance_position_gain; the same shared counter."),
    "stance_pressure_ref": (
        "S1", "mag", "direct", [("deep-stance", "threat>0")],
        "threat= is 0.0 unless stance_pressure_ref > 0.0 AND "
        "pressure > 0.0 (deepstance.c:395-396); source-2-conditional, "
        "like stance_position_gain."),

    # ---------------- Site placement extensions (S3) ------------------
    "site_defence_weight": (
        "S3", "mag", "-", None,
        "deep_site_defence_bonus() logs deep-sitedef with factor=, "
        "total= and deep= beside stock's own inverted term "
        "(deepsite.c:1052), but at LOG_VERBOSE -- invisible at the "
        "server's default verbosity, where every deep-* metric line in "
        "the corpus lives. Needs: promote to deep_metric_log_level() "
        "or an aggregate paid/count field near deep-sitenaval's."),
    "site_naval_gate": (
        "S3", "gate", "direct", [("deep-sitenaval", "refused>0")],
        "the A/B geometry switch for the maritime premium; refused= "
        "counts candidates the gate zeroed for lacking open water "
        "(deepsite.c:1085), zero on every line at the compiled gate=0 "
        "default."),
    "site_naval_weight": (
        "S3", "mag", "direct", [("deep-sitenaval", "paid>0")],
        "paid= counts candidates that kept a nonzero "
        "deep_site_naval_bonus() term (deepsite.c:1089); requires this "
        "weight > 0 to ever be nonzero at all."),
    "site_naval_connect_weight": (
        "S3", "mag", "direct", [("deep-sitenaval", "connect>=1")],
        "connect= counts candidates paid the additive connectivity "
        "half of the split premium -- coastal AND on water that "
        "bridges to another continent -- and connectwant= is the score "
        "it added. Both are structurally zero at the compiled 0.0 "
        "default, which is the OFF arm of the split's A/B; the "
        "population they will price is already visible as "
        "paid= minus refused= on the same line."),

    # ---------------- Explorer fleet saturation (S1) -------------------
    "explore_explorer_claim": (
        "S1", "mag", "direct", [("deep-explore", "claimed>0")],
        "claimed= is (explorers+queued) * explore_explorer_claim * "
        "explore_hut_value, logged on every deep-explore line "
        "(deepexplore.c:257); nonzero whenever any explorer is fielded "
        "or queued."),
    "explore_salvage_gap": (
        "S1", "mag", "proxy", [("deep-salvage", "")],
        "one term of deep_explore_surplus()'s clamped surplus "
        "(deepexplore.c:334), which deep-salvage's surplus= reports "
        "whole; not separable from explore_retain_floor's term in the "
        "same expression."),
    "explore_retain_floor": (
        "S1", "mag", "proxy", [("deep-salvage", "")],
        "as explore_salvage_gap; the retention floor and the "
        "hysteresis gap both feed the one clamped surplus deep-salvage "
        "reports (deepexplore.c:322/335-336)."),

    # ---------------- CM solvency and growth-price extensions (S1) ----
    "cm_grow_price_blend": (
        "S1", "mag", "direct", [("deep-cmgrow", "fires=1")],
        "fires= is (cm_grow_price_blend * citizen >= build), logged on "
        "every grow-hard admission test deep_cm_grow_priced() runs "
        "(deepcm.c:323/330); 0 disables the test outright."),
    "cm_specialist_gold_gain": (
        "S1", "mag", "direct", [("deep-cmspec", "gold>2")],
        "gold= is cm_parameter.factor[O_GOLD] after "
        "deep_cm_solvency_factors() (deepcm.c:130-133); stock's own "
        "pinned baseline is 2, so gold>2 is only reachable through "
        "this gain firing on a real gold shortfall."),
    "cm_specialist_sci_gain": (
        "S1", "mag", "direct", [("deep-cmspec", "sci>2")],
        "as cm_specialist_gold_gain; sci= is factor[O_SCIENCE], "
        "baseline 2 (deepcm.c:145-148)."),
    "cm_war_growth_charge": (
        "S1", "mag", "direct",
        [("deep-cmpost", ""), ("deep-cmsolve", "")],
        "both lines are gated on cm_war_growth_charge > 0.0 "
        "(deepcm.c:683/766); deep-cmpost's bar= is cm_war_survival "
        "minus this charge times the counterfactual's cost."),

    # ---------------- Garrison-floor per-class weights (S2) -----------
    "floor_barb_weight": (
        "S2", "mag", "direct", [("deep-floor", "wbarb>0")],
        "wbarb= is this weight times deep_barb_city_scale() "
        "(deepdefence.c:1617), logged on every eligible garrison-floor "
        "city-turn."),
    "floor_sea_weight": (
        "S2", "mag", "direct", [("deep-floor", "wsea>0")],
        "wsea= is this weight times the exterior/interior/marines "
        "escalation (deepdefence.c:1636-1641)."),
    "floor_air_weight": (
        "S2", "mag", "direct", [("deep-floor", "wair>0")],
        "wair= is exactly this weight when the air class is live "
        "(deepdefence.c:1675); no other term multiplies it."),
    "floor_border_weight": (
        "S2", "mag", "direct", [("deep-floor", "wborder>0")],
        "wborder= is this weight times the R/(R+d) proximity shape "
        "(deepdefence.c:1666-1668)."),
    "floor_interior_weight": (
        "S2", "mag", "proxy", [("deep-floor", "wsea>0")],
        "one factor of wsea= alongside floor_sea_weight and "
        "floor_marines_mult (deepdefence.c:1636-1641); not separable "
        "from them on the line."),
    "floor_marines_mult": (
        "S2", "mag", "proxy", [("deep-floor", "wsea>0")],
        "as floor_interior_weight; the escalation divisor folded into "
        "the same wsea= term."),

    # ---------------- Barbarian ramp shape (S2) ------------------------
    "barb_anticipation_turns": (
        "S2", "mag", "direct", [("deep-barb", "pre=1")],
        "pre=1 marks a line where the ramp is live before onset "
        "(deepdefence.c:1153), which requires lead = "
        "barb_anticipation_turns > 0; lead itself is also echoed "
        "verbatim on every deep-barb line."),
    "barb_size_ref_cities": (
        "S2", "mag", "proxy", [("deep-barb", "scale>0")],
        "the reference-city count normalises deep_barb_empire_scale()'s "
        "denominator (deepdefence.c:1037-1039); scale= folds it in with "
        "rate, cwc and the empire's own city count, none separable."),

    # ---------------- Standalone fear/priority terms (S1/S2) ----------
    "birth_guard_gain": (
        "S2", "mag", "direct", [("deep-birth", "")],
        "deep_birth_guard_want() returns before logging when "
        "birth_guard_gain <= 0.0 (deepdefence.c:4059); want= is this "
        "gain times p*worth."),
    "defensive_techwall_weight": (
        "S2", "mag", "direct", [("deep-defwall", "")],
        "deep_defensive_techwall() returns before any city is read "
        "when the weight is <= 0.0 (deepdefence.c:1958); credit= is "
        "the fear value times the deficit times this weight."),
    "attacker_hold_damper": (
        "S2", "mag", "direct", [("deep-damper", "pump=attacker")],
        "the attacker form of deep-damper is gated on "
        "attacker_hold_damper > 0.0 (deepdefence.c:5408)."),
    "diplomat_pump_damper": (
        "S1", "mag", "direct", [("deep-damper", "pump=diplomat")],
        "the diplomat form of deep-damper is gated on "
        "diplomat_pump_damper > 0.0 (deepdefence.c:4690)."),

    # ---------------- Wonder race pricing (S4) -------------------------
    "wrace_margin_turns": (
        "S4", "mag", "proxy", [("deep-wrace", "theirs>=0")],
        "the ramp's scale (deepwonder.c:248); p= on deep-wrace is a "
        "function of it only when a rival is known (theirs>=0) -- "
        "uncontested and unraced candidates short-circuit p to "
        "1.0/0.0 before the scale is read."),
    "wrace_pwin_gain": (
        "S4", "mag", "-", None,
        "deep-wrace's factor= folds wrace_pwin_gain, wrace_eta_gain "
        "and wrace_obsolete_gain into one product (deepwonder.c:482-"
        "499); none is separable on the line and the predicate "
        "language has no less-than operator to test departure from "
        "1.0. All three ship at 0.0 = off. Needs: per-term factors "
        "(pwin_factor=, eta_factor=, obs_factor=) on deep-wrace."),
    "wrace_eta_gain": (
        "S4", "mag", "-", None,
        "as wrace_pwin_gain; the same folded factor= term."),
    "wrace_obsolete_gain": (
        "S4", "mag", "-", None,
        "as wrace_pwin_gain; the same folded factor= term."),
    "wrace_urgency_gain": (
        "S4", "mag", "direct", [("deep-wrace-feed", "")],
        "deep-wrace-feed is logged only when urg != 1.0 "
        "(deeptrade.c:1626), which requires wrace_urgency_gain > 0.0 "
        "and a live race we are ahead in."),

    # ---------------- Expiry-aware amortization (CONSEQ item 4) -------
    "expiry_building_gain": (
        "S1", "mag", "direct", [("deep-expiry", "has:impr")],
        "the building form of deep-expiry is gated on "
        "expiry_building_gain > 0.0 (deephorizon.c:2043); keep= is "
        "1.0 - gain*(1-share)."),
    "expiry_unit_gain": (
        "S2", "mag", "direct", [("deep-expiry", "has:unit")],
        "the unit form of deep-expiry is gated on expiry_unit_gain > "
        "0.0 (deepdefence.c:4994)."),
    "expiry_unit_gap": (
        "S2", "mag", "direct", [("deep-expiry", "has:gap")],
        "gap= echoes the knob's own value on the same line "
        "(deepdefence.c:5010/5019); gated behind expiry_unit_gain "
        "also being > 0 (deepdefence.c:4994), so this knob's own "
        "branch never runs at expiry_unit_gain = 0."),

    # ---------------- Emergency treasury rungs (S2/S3) ----------------
    "sell_defend_gain": (
        "S2", "mag", "direct", [("deep-selldefend", "")],
        "deep_sell_to_defend() returns before reading anything when "
        "sell_defend_gain <= 0.0 (deepdefence.c:5544)."),
    "late_buy_gate": (
        "S2", "gate", "direct", [("deep-latebuy", "")],
        "deep_late_defence_buy() returns before reading a city when "
        "late_buy_gate <= 0 (deepdefence.c:5660)."),
    "evacuate_gate": (
        "S3", "gate", "direct", [("deep-evac", "")],
        "deep_evacuate_civilians() returns before reading a unit when "
        "evacuate_gate <= 0 (deepdefence.c:3140)."),

    # ---------------- Ground-holding: bases, fortify, pillage (S2/S3) -
    "base_fort_gain": (
        "S3", "mag", "direct", [("deep-fort", "wants>0")],
        "wants= counts deep_base_want() calls that returned positive "
        "(deepsettler.c:2074/2111), which requires base_fort_gain > "
        "0.0; offers= counts candidates regardless of the gain and "
        "does not evidence it."),
    "fortify_ground": (
        "S3", "gate", "direct", [("deep-ground", "")],
        "deep_fortify_ground() returns before reading a unit when "
        "fortify_ground <= 0 (deepdefence.c:3545)."),
    "pillage_denial_gain": (
        "S2", "mag", "direct", [("deep-pillage", "started>0")],
        "deep_pillage_sorties() returns before reading a unit when "
        "pillage_denial_gain <= 0.0 (deepdefence.c:3434); started= "
        "counts pillages the priced want actually cleared. nearby= "
        "can be nonzero on the same gated line from the raid_radius "
        "census alone, so started>0 is the tighter proof."),

    # ---------------- Sortie prize and deterrence dividend (S2) -------
    "sortie_prize_gain": (
        "S2", "mag", "direct", [("deep-sortie-no", "")],
        "the decline census only logs when priced = "
        "(sortie_prize_gain > 0.0) (deepdefence.c:2369/2450); the "
        "accepted-sortie line (deep-sortie) fires either way and does "
        "not by itself evidence this gain."),
    "deterrence_gain": (
        "S2", "mag", "direct", [("deep-deter", "")],
        "deep_deterrence_dividend() returns 0 before any DP run when "
        "deterrence_gain <= 0.0 (deepdefence.c:4190)."),

    # ---------------- Barracks streams and pop insurance (M4b) --------
    "unit_upkeep_charge": (
        "S2", "mag", "direct", [("deep-upcharge", "")],
        "deep_unit_upkeep_charge() returns 0 before pricing anything "
        "when unit_upkeep_charge <= 0.0 (deepdefence.c:6189)."),
    "pop_insurance_gain": (
        "S1", "mag", "direct", [("deep-popins", "")],
        "deep-popins only logs when deep_popins_cands > 0, which "
        "requires pop_insurance_gain > 0.0 (deephorizon.c:1445)."),
    "heal_stream_gain": (
        "S1", "mag", "direct", [("deep-barracks", "heal>0")],
        "heal= is deep_heal_cands, incremented only after "
        "heal_stream_gain > 0.0 passed (deephorizon.c:1236/1289)."),
    "veteran_stream_gain": (
        "S1", "mag", "direct", [("deep-barracks", "vet>0")],
        "vet= is deep_vet_cands, incremented only after "
        "veteran_stream_gain > 0.0 passed (deephorizon.c:1331/1391)."),

    # ---------------- Spy action valuation (W35 E35.1) -----------------
    "spy_enabled": (
        "S2", "mode", "direct", [("deep-spy-act", "")],
        "off-switch for acting on deep's own spy prices: "
        "deep_spy_manage() logs deep-spy-act only past the `if (!on) "
        "continue;` gate (deepspy.c:448); the deep-spy census line "
        "runs unconditionally and does not evidence this knob."),
    "spy_gold_value": (
        "S2", "mag", "direct", [("deep-spy-act", "kind=steal")],
        "the steal-tech value is exactly "
        "deep_spy_gold_worth(3*bulbs) (deepspy.c:139), so want= on a "
        "kind=steal line is linear in this rate; the embassy kind "
        "prices off embassy_science_value instead."),
    "spy_sabotage_gain": (
        "S2", "mag", "direct", [("deep-spy-act", "kind=sabotage")],
        "want= on a kind=sabotage line is spy_sabotage_gain times the "
        "target's shield stock or the improvement's build cost "
        "(deepspy.c:226/235)."),

    # ---------------- Trade route risk, love and partner-sparing (S4) -
    "trade_ceasefire_peace_odds": (
        "S4", "mag", "proxy", [("deep-trade-merit", "trunc>0")],
        "the tail-recovery term deep_route_price() adds only when a "
        "candidate's life was cease-fire-truncated (deeptrade.c:824-"
        "826); trunc= counts truncated candidates seen this call, not "
        "necessarily the one whose npv= is on the line, and the term "
        "is exactly zero at the compiled odds=0.0 default regardless "
        "of trunc."),
    "trade_route_risk_gain": (
        "S4", "mag", "direct", [("deep-trade-merit", "risk>0")],
        "risk= is 1 - survival, and survival stays 1.0 (risk=0) unless "
        "trade_route_risk_gain > 0.0 (deeptrade.c:1167-1173/1215)."),
    "trade_route_love": (
        "S4", "mag", "direct", [("deep-trade-love", "")],
        "deep_trade_route_love() returns before reading a route when "
        "trade_route_love <= 0.0 (deeptrade.c:413)."),
    "trade_partner_target_discount": (
        "S4", "mag", "direct", [("deep-leaders", "spared>0")],
        "spared= is dp->trade_partner_spared, incremented only when "
        "the discount actually multiplied a target's score "
        "(deepops.c:2644-2645), which requires the knob > 0.0 to set "
        "`spare` at all (deepops.c:2588)."),

    # ---------------- Diplomacy pre-pass: PEACE and BALANCE -----------
    "peace_seek_gain": (
        "S1", "mag", "direct",
        [("deep-peaceseek", ""), ("deep-peacetake", "")],
        "deep_peace_seek() and the treaty-side accept both run only "
        "when peace_seek_gain > 0.0 (deepdiplomacy.c:1446/1376); "
        "bar= on deep-peaceseek is req_love_for_peace * (1 - gain)."),
    "embassy_price_gain": (
        "S1", "mag", "direct", [("deep-embassy", "")],
        "deep_embassy_offer() and the treaty-side clause pricing both "
        "run only when embassy_price_gain > 0.0 (deepdiplomacy.c:1457/"
        "1395)."),
    "embassy_give_gain": (
        "S1", "mag", "direct", [("deep-embassy", "give=1")],
        "worth= on a give=1 line is -embassy_price_gain * "
        "embassy_give_gain * deep_embassy_leak_worth() "
        "(deepdiplomacy.c:258-259); the give=0 (receiving) line does "
        "not carry this gain at all."),
    "leader_check_gain": (
        "S2", "mag", "direct", [("deep-leaders", "applied>0")],
        "applied= is dp->leader_bias_applied, incremented only when "
        "leader_check_gain > 0.0 AND the target is the runaway leader "
        "(deepops.c:2629-2632)."),

    # ---------------- Endgame score mode and horizon credits (S1) -----
    "endgame_horizon": (
        "S1", "gate", "direct",
        [("deep-endgame", "clamps>0"), ("deep-endgame", "missed>0")],
        "clamps= (deep_horizon_turns()) and missed= (deep_arrives_in_"
        "time()) both stay 0 unconditionally when endgame_horizon <= "
        "0 (deephorizon.c:111/138)."),
    "endgame_window": (
        "S1", "mag", "direct",
        [("deep-endgame", "refused>0"), ("deep-endgame", "paid>0")],
        "deep_endgame_bias_buildings() returns before reading a city "
        "unless deep_in_endgame_window() is true, which requires "
        "endgame_window > 0 (deephorizon.c:162-164/1666)."),
    "endgame_unfinished_gain": (
        "S1", "mag", "direct", [("deep-endgame", "refused>0")],
        "refused= counts the deflator branch (mult = "
        "endgame_unfinished_gain) taking a want negative "
        "(deephorizon.c:1690/1706); a no-op at the compiled default "
        "1.0 (mult==1.0 is skipped, deephorizon.c:1697)."),
    "endgame_wonder_gain": (
        "S1", "mag", "direct", [("deep-endgame", "paid>0")],
        "paid= counts the great-wonder premium branch (mult = "
        "endgame_wonder_gain) taking a want up (deephorizon.c:1692/"
        "1708); a no-op at the compiled default 1.0."),
    "horizon_govcenter_weight": (
        "S1", "mag", "direct", [("deep-govcenter", "")],
        "deep-govcenter only logs when deep_govcenter_cities > 0, "
        "which requires horizon_govcenter_weight > 0.0 "
        "(deephorizon.c:891/1015)."),
    "horizon_upkeepfree_weight": (
        "S1", "mag", "direct", [("deep-upkeepfree", "")],
        "deep-upkeepfree only logs when deep_upkeepfree_cands > 0, "
        "which requires horizon_upkeepfree_weight > 0.0 "
        "(deephorizon.c:1066)."),
    "horizon_culture_weight": (
        "S1", "mag", "direct", [("deep-culture", "")],
        "deep_culture_credit() returns 0 before reading a city when "
        "horizon_culture_weight <= 0.0 (deephorizon.c:1488)."),
    "buy_per_gold": (
        "S2", "gate", "-", None,
        "switches deep_spend_gold()'s buy-list ranking from raw want "
        "to want/buycost (deepdefence.c:5819); the resulting buy is "
        "logged on deep-buy (shields=, the exact metric the ranking "
        "is meant to move) but that line is emitted 'ungated so the "
        "two ranking policies are comparable' (deepdefence.c:5956-"
        "5958) -- no field on it says which comparator picked the "
        "winner. Needs: a rank= or key= marker on deep-buy."),

    # ---------------- Conquest culture and the gift/proxy levers ------
    "conquest_culture_gain": (
        "S2", "mag", "direct", [("deep-conqculture", "")],
        "deep-conqculture only logs when deep_conq_culture_cands > 0, "
        "which requires conquest_culture_gain > 0.0 (deepops.c:132/"
        "145)."),
    "gift_border_value": (
        "S1", "mag", "direct", [("deep-gift", "")],
        "deep_gift_probe() is called only when gift_border_value > "
        "0.0 (deepdiplomacy.c:1449); border= on the line is pressure "
        "times this rate."),
    "gift_love_gain": (
        "S1", "mag", "direct", [("deep-lovegift", "")],
        "deep_love_gift() is called only when gift_love_gain > 0.0 "
        "(deepdiplomacy.c:1453)."),
    "gift_ally_gain": (
        "S1", "mag", "direct", [("deep-lovegift", "rel=ally")],
        "GIFT-REFOLD (11.4): rel= on deep-lovegift is "
        "deep_gift_rel_of()'s class and relgain= is literally this "
        "knob on an ally-class line; bound= is gift_love_gain * worth "
        "* relgain, and refused=1 counts the gifts this class price "
        "declined."),
    "gift_instrument_gain": (
        "S1", "mag", "direct", [("deep-lovegift", "rel=instr")],
        "GIFT-REFOLD (11.4): the instrumental class -- no friendship, "
        "but we are at war elsewhere and they press our border. "
        "relgain= on a rel=instr line is this knob."),
    "gift_rival_gain": (
        "S1", "mag", "direct", [("deep-lovegift", "rel=rival")],
        "GIFT-REFOLD (11.4): the population that banked BALANCE's "
        "+5.26 counterparty surplus. relgain= on a rel=rival line is "
        "this knob; at 0.0 every such line carries refused=1 took=0."),
    "proxy_arm_gain": (
        "S1", "mag", "direct", [("deep-proxy", "")],
        "deep_proxy_arm() is called only when proxy_arm_gain > 0.0 "
        "(deepdiplomacy.c:1465); budget= on the line is pressure * "
        "gift_border_value * this gain."),

    # ---------------- S5: honesty (omniscience experiment flag) -------
    "honest_info": (
        "S1", "mode", "direct", [("deep-honest", "")],
        "A STRUCTURAL EXPERIMENT FLAG (see its own comment, "
        "deeptuning.h:1868), not an optimiser knob. deep-honest only "
        "logs when imposed > 0, which requires honest_info > 0 "
        "(deepai.c:559/580); see findings/omniscience-census-2026-08-"
        "11.md."),
}

STAGE_NAME = {
    "S1": "ECON (stage 1)",
    "S2": "MIL (stage 2)",
    "S3": "SITE/EXPANSION (stage 3)",
    "S4": "TRADE (stage 4)",
}

# --------------------------------------------------------------------
# Log parsing
# --------------------------------------------------------------------

LINE = re.compile(rb"^\s*\d+:\s+(deep-[a-z0-9-]+)\b(.*)$")
CELLHASH = re.compile(r"^(.*)-[0-9a-f]{12}$")


def compile_pred(pred):
    """Compile a predicate to a callable on the rest-of-line bytes.

    Byte-level and single-pass: the corpus is gigabytes of log and a
    field-dictionary per line was the whole runtime.
    """
    if not pred:
        return lambda rest: True
    if pred.startswith("flag:"):
        word = pred[5:].encode()
        rx = re.compile(rb"(?<![-\w=])" + re.escape(word) + rb"(?![-\w=])")
        return lambda rest: rx.search(rest) is not None
    if pred.startswith("has:"):
        rx = re.compile(rb"\b" + re.escape(pred[4:].encode()) + rb"=")
        return lambda rest: rx.search(rest) is not None
    for op in (">=", "!=", ">", "="):
        if op in pred:
            key, val = (s.strip() for s in pred.split(op, 1))
            if op in ("=", "!="):
                rx = re.compile(rb"\b" + re.escape(key.encode()) + rb'="?'
                                + re.escape(val.encode()) + rb'"?(?![\w.])')
                if op == "=":
                    return lambda rest: rx.search(rest) is not None
                present = re.compile(rb"\b" + re.escape(key.encode()) + rb"=")
                return lambda rest: (present.search(rest) is not None
                                     and rx.search(rest) is None)
            rx = re.compile(rb"\b" + re.escape(key.encode())
                            + rb"=(-?[0-9]+(?:\.[0-9]+)?)")
            thr = float(val)
            if op == ">":
                return lambda rest: bool(
                    (m := rx.search(rest)) and float(m.group(1)) > thr)
            return lambda rest: bool(
                (m := rx.search(rest)) and float(m.group(1)) >= thr)
    raise SystemExit(f"bad predicate: {pred!r}")


# --------------------------------------------------------------------
# Corpus walk
# --------------------------------------------------------------------

class Cell:
    def __init__(self, name, root):
        self.name = name
        self.roots = {root}
        self.games = 0
        self.tags = collections.Counter()       # tag -> lines seen
        self.hits = collections.Counter()       # (knob, i) -> lines matched
        self.servers = set()
        self.tunings = set()
        self.quiet_games = 0

    def merge_key(self, keytxt):
        for line in keytxt.splitlines():
            if line.startswith("server="):
                sha = line.split("sha256=")[-1].strip()
                self.servers.add(sha[:12])
            elif line.startswith("tuning="):
                path = line.split("=", 1)[1].split(" sha256")[0]
                self.tunings.add(Path(path).name if path != "none" else "none")


def cell_name(p):
    m = CELLHASH.match(p.name)
    return m.group(1) if m else p.name


def probes_for(knob):
    ent = MAP[knob]
    return ent[3] or []


def build_probes(only_knob):
    """tag -> [(knob, probe index, compiled predicate), ...]"""
    by_tag = collections.defaultdict(list)
    for knob, ent in MAP.items():
        if only_knob and knob != only_knob:
            continue
        for i, (tag, pred) in enumerate(ent[3] or []):
            by_tag[tag].append((knob, i, compile_pred(pred)))
    return by_tag


def scan_log(path, by_tag):
    """One game.log -> (tag counter, (knob, i) counter)."""
    tags = collections.Counter()
    hits = collections.Counter()
    get = by_tag.get
    with open(path, "rb") as fh:
        for raw in fh:
            if b"deep-" not in raw:
                continue
            m = LINE.match(raw)
            if m is None:
                continue
            tag = m.group(1).decode()
            tags[tag] += 1
            probes = get(tag)
            if not probes:
                continue
            rest = m.group(2)
            for knob, i, pred in probes:
                if pred(rest):
                    hits[(knob, i)] += 1
    return tags, hits


def collect_logs(roots, seeds_cap):
    out = []
    per_arm = collections.Counter()
    for root in roots:
        for log in sorted(Path(root).rglob("game.log")):
            arm = log.parent.parent
            celldir = arm.parent
            if seeds_cap:
                key = (str(celldir), arm.name)
                per_arm[key] += 1
                if per_arm[key] > seeds_cap:
                    continue
            out.append((str(root), log, celldir))
    return out


_BY_TAG = None


def _worker_init(only_knob):
    global _BY_TAG
    _BY_TAG = build_probes(only_knob)


def _worker(path):
    return scan_log(path, _BY_TAG)


def scan(roots, seeds_cap, only_knob, jobs=1):
    by_tag = build_probes(only_knob)
    logs = collect_logs(roots, seeds_cap)
    cells = {}

    def cell_for(root, celldir):
        name = cell_name(celldir)
        cell = cells.get(name)
        if cell is None:
            cell = cells[name] = Cell(name, root)
        cell.roots.add(root)
        keyf = celldir / "key.txt"
        if keyf.exists():
            cell.merge_key(keyf.read_text(errors="replace"))
        return cell

    if jobs > 1:
        import multiprocessing as mp
        # 'fork': the default forkserver needs an AF_UNIX socket, which
        # a sandboxed shell may refuse.
        ctx = mp.get_context("fork")
        with ctx.Pool(jobs, initializer=_worker_init,
                      initargs=(only_knob,)) as pool:
            results = pool.map(_worker, [str(p) for _r, p, _c in logs],
                               chunksize=4)
    else:
        results = [scan_log(str(p), by_tag) for _r, p, _c in logs]

    for (root, _p, celldir), (tags, hits) in zip(logs, results):
        cell = cell_for(root, celldir)
        cell.games += 1
        if not tags or set(tags) <= {"deep-tuning"}:
            # Only deep-tuning present: the game ran under
            # FREECIV_DEEP_QUIET, or this arm is not ai/deep at all.
            cell.quiet_games += 1
        cell.tags.update(tags)
        cell.hits.update(hits)
    return cells


# --------------------------------------------------------------------
# Verdicts
# --------------------------------------------------------------------

def verdict(knob, cells):
    ent = MAP[knob]
    kind, grade, probes = ent[1], ent[2], ent[3]
    if probes is None:
        return "NO-INSTRUMENT", [], []
    live, readable = [], []
    for name, cell in sorted(cells.items()):
        if cell.quiet_games == cell.games:
            continue                      # DEEP_QUIET: no evidence either way
        tags = {t for (t, _p) in probes}
        if not any(cell.tags.get(t) for t in tags):
            continue                      # tag absent: binary may predate it
        readable.append(name)
        n = sum(cell.hits.get((knob, i), 0) for i in range(len(probes)))
        if n:
            live.append((name, n))
    if kind == "mode":
        return "MODE", live, readable
    if live:
        return "LIVE", live, readable
    return ("DEAD" if readable else "NO-CORPUS"), live, readable


def load_knobs():
    order = []
    for line in PARAMS.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith(";") or line.startswith("["):
            continue
        order.append(line.split("=")[0].strip())
    return order


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--md", action="store_true")
    ap.add_argument("--seeds", type=int, default=0)
    ap.add_argument("--knob")
    ap.add_argument("--dirs-only", action="store_true", dest="dirs_only")
    ap.add_argument("--jobs", type=int, default=1)
    args = ap.parse_args()

    order = load_knobs()
    missing = [k for k in order if k not in MAP]
    extra = [k for k in MAP if k not in order]
    if missing or extra:
        sys.exit(f"map out of sync with tuning-params.ini:\n"
                 f"  unmapped knobs: {missing}\n"
                 f"  stale map keys: {extra}")

    cells = scan(args.dirs, args.seeds, args.knob, args.jobs)

    if args.dirs_only:
        print(f"{'cell':28s} {'games':>6s} {'nolines':>8s} "
              f"{'servers':26s} tunings")
        for name, c in sorted(cells.items()):
            print(f"{name:28s} {c.games:6d} {c.quiet_games:6d} "
                  f"{','.join(sorted(c.servers)):26s} "
                  f"{','.join(sorted(c.tunings))}")
        return

    rows = []
    for knob in order:
        if args.knob and knob != args.knob:
            continue
        stage, kind, grade, probes, note = MAP[knob]
        v, live, readable = verdict(knob, cells)
        instr = ("-" if probes is None
                 else " | ".join(f"{t} {p}".strip() for t, p in probes))
        rows.append((knob, stage, kind, grade, instr, v, live, readable, note))

    counts = collections.Counter(r[5] for r in rows)
    if args.md:
        print("| knob | block | instrument | grade | cells firing | verdict |")
        print("|---|---|---|---|---|---|")
        for (knob, stage, kind, grade, instr, v, live, readable,
             _n) in rows:
            shown = [f"{n} ({c})" for n, c in
                     sorted(live, key=lambda t: -t[1])[:5]]
            more = len(live) - len(shown)
            cellstr = ", ".join(shown) + (f", +{more} more" if more else "")
            if live:
                cellstr = f"{len(live)}/{len(readable)}: " + cellstr
            elif v == "DEAD":
                cellstr = f"0/{len(readable)}"
            else:
                cellstr = "--"
            # A pipe inside a cell would end the markdown column.
            safe = instr.replace("|", "\\|")
            print(f"| `{knob}` | {stage} | `{safe}` | {grade} | "
                  f"{cellstr} | **{v}** |")
        print()
    else:
        w = max(len(r[0]) for r in rows)
        for (knob, stage, kind, grade, instr, v, live, readable,
             _n) in rows:
            cellstr = ",".join(f"{n}:{c}" for n, c in live) or "-"
            print(f"{knob:{w}s} {stage} {grade:6s} {v:14s} "
                  f"{instr[:44]:44s} {cellstr[:80]}")
        print()
    total_games = sum(c.games for c in cells.values())
    readable_cells = [n for n, c in sorted(cells.items())
                      if c.quiet_games != c.games]
    print(f"corpus: {len(cells)} cells, {total_games} game logs, "
          f"{len(readable_cells)} cells with metric lines")
    print("verdicts: " + "  ".join(f"{k}={v}" for k, v in
                                   sorted(counts.items())))


if __name__ == "__main__":
    main()
