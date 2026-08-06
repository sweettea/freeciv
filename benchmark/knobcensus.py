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
        "S3", "mag", "-", None,
        "deep_expansion_blocked() returns a bool nobody logs "
        "(deepdefence.c:2294). Needs: a blocked-expansion counter."),
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
        "S1", "mag", "-", None,
        "the discounted-hut subset is folded into hut_weight and never "
        "logged (deepexplore.c:133). Needs: huts_discounted= on "
        "deep-explore."),
    "hut_risk_discount": (
        "S1", "mag", "-", None, "as hut_risk_radius; same line, same need."),
    "rates_science_gain": (
        "S1", "mag", "direct", [("deep-rates", "vsci>0")],
        "deep-rates logs vsci, the science valuation this gain scales "
        "(deeprates.c:410). Inert while rates_enabled = 0."),
    "rates_luxury_gain": (
        "S1", "mag", "direct", [("deep-rates", "vlux>0")],
        "deep-rates logs vlux (deeprates.c:418). Inert while "
        "rates_enabled = 0."),

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
        "S2", "mag", "direct", [("deep-invade", "")],
        "deep-invade's p= is deep_take_odds_at_arrival(), whose "
        "reinforcement model this rate is (deepcombat.c:438)."),
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
