#!/usr/bin/env python3
"""Zero-game knob sensitivity screen: which tunable can move a DECISION.

`knobcensus.py` answers "can this knob's mechanism be SEEN executing".
This answers the strictly harder question the next tuning campaign
needs: **if the knob moves by one mutation step, does any logged
decision come out differently?**  A knob that cannot flip a decision in
40 games cannot be fit by 8 noisy seeds either, and every generation
spent mutating it is a generation of pure order-statistic noise
(findings/tune-postfix-verdict-2026-08-25.md).

METHOD -- no new games, ever.

  1. Every knob's step is tune.py's own mutation sigma, `range/6`
     (tune.py mutate()).  The screen perturbs to `default +- sigma`,
     clipped to [min, max], and for integer knobs rounds.
  2. The BASE value is read from the game's own `deep-tuning` line, not
     from the compiled defaults, so a corpus taken under a tuning file
     is screened around the values it actually ran.
  3. For each REPLAY RULE below, the logged line (or a small join of
     logged lines) carries every input of a decision, so the decision is
     recomputed at base and at base +- sigma and the outcomes compared.
     A FLIP is a change in a DISCRETE outcome the log itself reports: a
     verdict, an auction winner, a gate, or an integer count.
  4. Knobs with no rule are CLASSIFIED, not guessed at.  The classes
     are stated per knob in the output and the reason is knobcensus's
     hand-built instrument map, which was written by reading the
     enclosing function of every consumer site.

CLASSES

  reconstructible  a replay rule exists: the log carries every operand
                   of the comparison the knob enters, so the flip count
                   is a measurement.
  partial          an instrument fires on the knob's path, but the log
                   reports the mechanism running and not both sides of
                   the comparison.  A value shift is computable, a flip
                   is not.
  not-logged       nothing in ai/deep evidences this knob's arithmetic
                   at the metric log level (knobcensus NO-INSTRUMENT).
  dead-path        an instrument EXISTS and its tag never appeared in
                   this corpus -- the knob's mechanism did not run in
                   40 games.  Usually a gate compiled to 0.
  mode             a switch or degenerate range: pinned per experiment,
                   never optimised.  Reported with the flip count its
                   OTHER setting would produce where that is
                   reconstructible.

WHAT A FLIP COUNT IS NOT.  It is not an effect size.  A knob that flips
many decisions may still be worthless; a knob that flips none is
provably worthless to an optimiser working from these venues.  The
screen is a NECESSARY-condition filter, and that is the whole claim.

Usage
    knobscreen.py CORPUS_DIR [CORPUS_DIR ...] [--md] [--jobs N]
                  [--seeds N] [--rule NAME] [--validate]

    --md        markdown tables for the findings doc
    --validate  print, per replay rule, how often the base-value replay
                agrees with the outcome the log recorded, which is the
                only check that the reconstruction is the real formula

CORPUS VINTAGE.  Three log fields postdate the 2026-08-30 screen and
seven knobs are screenable only on a corpus that carries them:

  deep-decay-sum bund/cont/mult/grow  horizon_bundle_weight,
                                      horizon_content_weight,
                                      horizon_multiplier_weight,
                                      horizon_growth_weight
  deep-bid       milclass             scale_defender, scale_attacker,
                                      scale_ferry
  deep-milcap    (whole tag)          emergency_value_cap, at its own
                                      seam rather than at the merge

On an older corpus the fields are simply absent, every rule still
replays the knobs it always could, and the seven above are listed under
UNMEASURED rather than silently reported as flipping nothing.  The
distinction matters: "this knob flips no decision" and "this corpus
cannot see this knob" are different claims and the screen must not
conflate them.
"""

import argparse
import collections
import io
import math
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PARAMS = HERE / "tuning-params.ini"

sys.path.insert(0, str(HERE))
try:
    from knobcensus import MAP as CENSUS_MAP, compile_pred
except Exception:                                        # pragma: no cover
    CENSUS_MAP, compile_pred = {}, None

# ai/default constants the replays need, read from the tree rather than
# retyped: a screen that invents a constant is rule 7 twice over.
DAI_WANT_BELOW_MIL_EMERGENCY = 1000.0
DAI_WANT_MILITARY_EMERGENCY = DAI_WANT_BELOW_MIL_EMERGENCY + 0.1
DAI_WANT_DOMESTIC_MAX = DAI_WANT_MILITARY_EMERGENCY / 4 * 3   # 750.075
SHIELD_WEIGHTING = 17.0                       # advbuilding.h:20
TRAIT_DEFAULT_VALUE = 50                      # traits.h

LINE = re.compile(rb"(deep-[a-z0-9-]+) (.*)")

# Fields (and one whole tag) that postdate the 2026-08-30 screen, and
# the knobs that are screenable only where they are present.  Reported
# as UNMEASURED on an older corpus rather than as zero flips.
NEW_FIELDS = {
    ("deep-decay-sum", "bund"): ["horizon_bundle_weight"],
    ("deep-decay-sum", "cont"): ["horizon_content_weight"],
    ("deep-decay-sum", "mult"): ["horizon_multiplier_weight"],
    ("deep-decay-sum", "grow"): ["horizon_growth_weight"],
    ("deep-bid", "milclass"): ["scale_defender", "scale_attacker",
                               "scale_ferry"],
    ("deep-milcap", "pre"): ["emergency_value_cap"],
}
FIELD = re.compile(rb'([a-z_0-9]+)=("[^"]*"|\(-?\d+,-?\d+\)|[^\s]+)')


# --------------------------------------------------------------------
# Knob table
# --------------------------------------------------------------------

class Knob:
    __slots__ = ("name", "default", "lo", "hi", "kind", "sigma")

    def __init__(self, name, default, lo, hi, kind):
        self.name, self.kind = name, kind
        self.default, self.lo, self.hi = default, lo, hi
        self.sigma = (hi - lo) / 6.0

    def steps(self, base):
        """[(label, value), ...] -- base +- one mutation sigma.

        Degenerate ranges (lo == hi) have no step; a 0/1 int is a MODE
        switch and its 'step' is the other setting."""
        if self.hi <= self.lo:
            return []
        if self.is_mode():
            other = self.hi if base <= self.lo else self.lo
            return [("alt", other)]
        out = []
        for label, v in (("-1s", base - self.sigma), ("+1s", base + self.sigma)):
            v = min(self.hi, max(self.lo, v))
            if self.kind == "int":
                v = float(round(v))
            if v != base:
                out.append((label, v))
        return out

    def is_mode(self):
        return self.kind == "int" and self.lo == 0 and self.hi == 1


def load_knobs():
    knobs, order = {}, []
    for line in PARAMS.read_text().splitlines():
        line = line.split(";")[0].strip()
        if not line or line.startswith("["):
            continue
        name, rest = (s.strip() for s in line.split("=", 1))
        parts = [p.strip() for p in rest.split(",")]
        if len(parts) < 4:
            continue
        d, lo, hi, kind = float(parts[0]), float(parts[1]), float(parts[2]), parts[3]
        knobs[name] = Knob(name, d, lo, hi, kind)
        order.append(name)
    return knobs, order


# --------------------------------------------------------------------
# Replay rules
#
# Each rule declares:
#   tags    the log tags it consumes
#   knobs   the knobs whose sensitivity it measures
#   join    None for a per-line rule, or a key function producing a
#           join key so several tags of one city-turn arrive together
#   decide(ev, t) -> hashable decision, or None to skip this event
#           `t` is a dict {knob: value}; `ev` is the parsed line (or,
#           for a join rule, a dict tag -> line)
#   truth(ev) -> the outcome the LOG recorded, for --validate, or None
# --------------------------------------------------------------------

RULES = {}


def rule(name, tags, knobs, join=None, note=""):
    def deco(fn):
        RULES[name] = dict(name=name, tags=tuple(tags), knobs=tuple(knobs),
                           join=join, decide=fn, note=note, check=None)
        return fn
    return deco


def check_of(name):
    """Register the rule's SELF-CHECK: check(ev, t0) is True when the
    base-value replay reproduces the outcome the log itself recorded,
    False when it does not, None when the line carries no check."""
    def deco(fn):
        RULES[name]["check"] = fn
        return fn
    return deco


def f(ev, key, default=0.0):
    v = ev.get(key)
    return default if v is None else v


# ---- R1: operation formation verdict (deepops.c:4243/4300) ----------
# The scoring loop takes the smallest pool prefix whose take-odds reach
# `bar`; `best_odds` is the best prefix odds it saw, so the formation
# fires iff best_odds >= bar.  `bar` is the stance-scaled bar and
# `take_target` the unscaled knob beside it, so the stance factor is
# bar/take_target and survives the perturbation exactly.

@rule("op_formation", ["deep-opodds"], ["op_take_target"],
      note="verdict formed/muster vs refused; bar = stance x op_take_target")
def _r_op(ev, t):
    verdict = ev.get("verdict")
    if verdict not in ("formed", "refused", "muster"):
        return None
    take = ev.get("take_target")
    bar = ev.get("bar")
    if take is None or bar is None or take <= 0:
        return None
    scale = bar / take
    return f(ev, "odds") >= scale * t["op_take_target"]


@check_of("op_formation")
def _c_op(ev, t):
    return _r_op(ev, t) == (ev.get("verdict") in ("formed", "muster"))


# ---- R2: expansion gate (deepdefence.c deep_expansion_blocked) ------
# blocked = wave > 0 && surv < expansion_war_survival.  wave < 0 marks
# the paths that never reached the comparison.

@rule("expansion_gate", ["deep-expgate"], ["expansion_war_survival"],
      note="blocked = wave>0 && surv < expansion_war_survival")
def _r_expgate(ev, t):
    if f(ev, "wave", -1) <= 0:
        return None
    return f(ev, "surv") < t["expansion_war_survival"]


@check_of("expansion_gate")
def _c_expgate(ev, t):
    return _r_expgate(ev, t) == (f(ev, "blocked") > 0)


# ---- R3: the city-turn auction (deepdefence.c deep_manage_cities) ---
# Three logged domestic bidders (founder, worker, building anchor), the
# four transforms domestic_advisor_choose_build() applies to the
# founder, and the (tier, value) merge against deep's military choice.
# deep-bid and deep-founderbid are emitted at the same seam, before
# anything is rescaled, so the join is exact.
#
# NOT modelled, and stated rather than hidden: the wonder-caravan and
# trade-caravan producers bid in the same advisor and are not logged.
# The want corpus (findings/want-corpus-2026-08-06.md S2) puts them at
# 5.7 and 0.2 wins per game against the building anchor's 1249, so the
# reconstruction misses a low-single-digit share of winners; --validate
# reports the residual.

def _founder_transform(want, ev):
    """daidomestic.c:545-560 on an int want, with the logged branches."""
    w = int(want)
    if f(ev, "wonder") > 0:
        # pop_cost decides /5 vs /2 and is not logged; recover it from
        # the bid the line itself recorded rather than assuming.
        div = 5
        for cand in (5, 2):
            probe = int(w / cand)
            if f(ev, "capped") > 0:
                probe = int(probe / 100)
            probe *= f(ev, "trait", TRAIT_DEFAULT_VALUE) / TRAIT_DEFAULT_VALUE
            if abs(probe - f(ev, "bid")) < 0.51:
                div = cand
                break
        w = int(w / div)
    if f(ev, "capped") > 0:
        w = int(w / 100)
    return w * f(ev, "trait", TRAIT_DEFAULT_VALUE) / TRAIT_DEFAULT_VALUE


def _auction(j, t):
    """(winner-class, domestic-want) for one city-turn, or None."""
    bid, fb = j.get("deep-bid"), j.get("deep-founderbid")
    if bid is None or fb is None:
        return None
    fs, ws = t["scale_settler"], t["scale_worker"]
    founder = f(bid, "founder")
    worker = f(bid, "worker")
    if fs != 1.0 and founder != 0:
        founder = float(int(founder * fs))
    if ws != 1.0 and worker != 0:
        worker = float(int(worker * ws))

    # worker: scaled by `expand`, food-vetoed, positive only
    want, who = 0.0, "none"
    wwant = worker * f(bid, "expand", 100) / 100.0
    if wwant > 0 and f(fb, "fed") > 0:
        want, who = wwant, "worker"

    # founder: four transforms, then a strict > against the incumbent
    fwant = _founder_transform(founder, fb)
    if f(fb, "fed") > 0:
        if fwant > want:
            want, who = fwant, "founder"
        elif fwant < -want:
            want, who = -fwant, "founder-boat"

    # building anchor, times the BUILDER trait (1.0 at default traits)
    build = f(bid, "build")
    if build > want:
        want, who = build, "building"

    return who, min(want, DAI_WANT_DOMESTIC_MAX)


@rule("auction_domestic", ["deep-bid", "deep-founderbid"],
      ["scale_settler", "scale_worker"],
      join=lambda ev: (ev.get("turn"), ev.get("city")),
      note="winner among {worker, founder, building} inside "
           "domestic_advisor_choose_build()")
def _r_auction_dom(j, t):
    r = _auction(j, t)
    return None if r is None else r[0]


# enum deep_want_class (deepwant.h): 0 building, 1 defender,
# 2 attacker, 3 ferry, 4 other.  deep_want_scale_apply() multiplies a
# NORMAL-tier military bid by its class's key and leaves the EMERGENCY
# tier alone, so `milclass` is the whole of what the three military
# scale keys needed to become screenable.
_MIL_SCALE = {1: "scale_defender", 2: "scale_attacker", 3: "scale_ferry"}


@rule("auction_merge", ["deep-bid", "deep-founderbid"],
      ["scale_settler", "scale_worker", "founder_tier_gate",
       "emergency_value_cap"] + sorted(set(_MIL_SCALE.values())),
      join=lambda ev: (ev.get("turn"), ev.get("city")),
      note="does the domestic winner beat the military choice on "
           "(tier, value)?")
def _r_auction_merge(j, t):
    r = _auction(j, t)
    if r is None:
        return None
    who, dom = r
    bid = j["deep-bid"]
    miltier = int(f(bid, "miltier"))
    mil = f(bid, "mil")
    cap = t["emergency_value_cap"]
    if miltier == 1 and cap > 0 and mil > cap:
        mil = cap
    # The class scale, NORMAL tier only and positive wants only, which
    # is deep_want_scale_apply()'s own guard.
    cls = bid.get("milclass")
    if cls is not None and miltier == 0 and mil > 0:
        knob = _MIL_SCALE.get(int(cls))
        if knob is not None:
            mil = mil * t[knob]
    # founder_tier_gate: deep_founder_contests_emergency() lets a
    # founder want that outranks an emergency military want on
    # MAGNITUDE take the build (deepdefence.c:7885).
    if (t["founder_tier_gate"] > 0 and f(bid, "founder") > 0
            and miltier == 1 and f(bid, "founder") > mil
            and who == "founder"):
        return True
    if miltier != 0:
        return False                      # emergency beats every normal
    return dom > mil


@check_of("auction_domestic")
def _c_auction_dom(j, t):
    """The founder leg only: does the modelled transform reproduce the
    bid daidomestic really formed?  The worker and building legs have no
    logged post-transform value to check against, so the agreement rate
    here validates one of the three legs and is reported as such."""
    fb = j.get("deep-founderbid")
    if fb is None or f(fb, "want") == 0:
        return None
    got = _founder_transform(f(fb, "want"), fb)
    return abs(got - f(fb, "bid")) < 1.0


# ---- R11: the building anchor (deephorizon.c) ----------------------
# The horizon credits are applied to stock's building_want[] array and
# the census logs, per credited improvement, stock's own `pre` and every
# modifier in application order, plus the best UNCREDITED competitor on
# deep-decay-city.  So the argmax the auction's building anchor takes is
# reconstructible for the whole city-turn.  Same reconstruction
# benchmark/decay-census.py uses for the decay-rate flip curve; this
# rule reuses it for the modifier knobs rather than the rate.
#
#   add = MAX(raw * gmult  - upkeep_per_turn * upkeep_gold_weight * bdur, 0)
#         then  * keep,  keep = 1 - expiry_building_gain * (1 - ob/bdur)
#   raw carries five named components, each of which is EXACTLY linear
#   in one horizon_*_weight -- amortize() multiplies its benefit by
#   pow(discount, delay) and deep_decay_reprice() by dur/steps, neither
#   of which reads the weight -- so a knob is re-priced by scaling its
#   own component and leaving the rest of `raw` alone.  The unnamed
#   remainder (culture, pop insurance, heal, veteran, health) is what
#   no single one of these knobs scales.

_HORIZON_COMPONENT = {
    "horizon_capacity_weight": "capadd",     # deephorizon.c:579
    "horizon_bundle_weight": "bund",         # deephorizon.c:1014
    "horizon_content_weight": "cont",        # deephorizon.c:704
    "horizon_multiplier_weight": "mult",     # deephorizon.c:935
    "horizon_growth_weight": "grow",         # deephorizon.c:772
}


def _decay_add(ev, t, base):
    raw = f(ev, "raw")
    for knob, field in _HORIZON_COMPONENT.items():
        w = t[knob]
        b = base.get(knob, 1.0)
        if w != b and b and field in ev:
            raw = raw + f(ev, field) * (w / b - 1.0)
    add = raw
    gmult = f(ev, "gmult", 1.0)
    if add > 0 and gmult != 1.0:
        scaled = add * gmult
        add = max(add, scaled) if f(ev, "thr") > 0 else scaled
    if add > 0:
        b_upkw = base.get("upkeep_gold_weight", 0.0)
        # `upk` is city_improvement_upkeep * upkeep_gold_weight; the
        # ruleset's own per-turn bill divides back out.
        per_turn = (f(ev, "upk") / b_upkw) if b_upkw else 0.0
        add = max(add - per_turn * t["upkeep_gold_weight"] * f(ev, "bdur"),
                  0.0)
    g = t["expiry_building_gain"]
    if add > 0 and g > 0.0 and f(ev, "ob", -1) >= 0:
        horizon = max(1.0, f(ev, "bdur"))
        share = min(1.0, f(ev, "ob") / horizon)
        add *= max(1.0 - g * (1.0 - share), 0.0)
    return add


@rule("building_anchor", ["deep-decay-sum", "deep-decay-city"],
      ["upkeep_gold_weight", "expiry_building_gain"]
      + list(_HORIZON_COMPONENT),
      join=lambda ev: (ev.get("turn"), ev.get("city")),
      note="argmax over credited improvements (pre+add) and the best "
           "uncredited competitor -- the anchor the settler bids against")
def _r_anchor(j, t):
    sums = j.get("deep-decay-sum")
    city = j.get("deep-decay-city")
    if sums is None:
        return None
    if not isinstance(sums, list):
        sums = [sums]
    best_name, best = None, (f(city, "bwant") if city else 0.0)
    if city:
        best_name = city.get("bestun")
    base = t["_base"]
    for ev in sums:
        v = f(ev, "pre") + _decay_add(ev, t, base)
        if v > best:
            best, best_name = v, ev.get("impr")
    return best_name


@check_of("building_anchor")
def _c_anchor(j, t):
    """Does the modifier chain reproduce every logged `add`?  This is
    the check that matters: the argmax is only as good as the per
    improvement credit that feeds it."""
    sums = j.get("deep-decay-sum")
    if sums is None:
        return None
    if not isinstance(sums, list):
        sums = [sums]
    for ev in sums:
        got = _decay_add(ev, t, t["_base"])
        want = f(ev, "add")
        if abs(got - want) > max(0.01, 0.002 * max(abs(want), 1.0)):
            return False
    return True


# ---- R4/R5/R6/R7: the four auction debits and credits ---------------
# All four run on both sides of the merge and log `who` (mil|dom),
# `want` and the modified want, so each is replayable per line and, when
# both sides fired in one city-turn, the argmax between them is too.

@rule("switch_cost", ["deep-switchcost"], ["switch_cost_gain"],
      note="left = max(want - forfeit*17*switch_cost_gain, 0); "
           "flip = the bid is annihilated or survives")
def _r_switchcost(ev, t):
    debit = f(ev, "forfeit") * SHIELD_WEIGHTING * t["switch_cost_gain"]
    return max(f(ev, "want") - debit, 0.0) > 0


@check_of("switch_cost")
def _c_switchcost(ev, t):
    return _r_switchcost(ev, t) == (f(ev, "left") > 0)


@rule("upkeep_charge", ["deep-upcharge"],
      ["unit_upkeep_charge", "upkeep_gold_weight"],
      note="debit = (ups*dur*17 + (upg+upf)*dur*W) * C; "
           "flip = the bid is annihilated or survives")
def _r_upcharge(ev, t):
    dur = f(ev, "dur")
    charge = (f(ev, "ups") * dur * SHIELD_WEIGHTING
              + (f(ev, "upg") + f(ev, "upf")) * dur * t["upkeep_gold_weight"])
    return max(f(ev, "want") - charge * t["unit_upkeep_charge"], 0.0) > 0


@check_of("upkeep_charge")
def _c_upcharge(ev, t):
    return _r_upcharge(ev, t) == (f(ev, "left") > 0)


@rule("prod_keep", ["deep-prodkeep"], ["prod_keep_gain"],
      note="credit = want*gain*progress, clamped below the tier line; "
           "flip = the credit exists at all")
def _r_prodkeep(ev, t):
    ceiling = DAI_WANT_BELOW_MIL_EMERGENCY - 1
    want = f(ev, "want")
    credit = want * t["prod_keep_gain"] * f(ev, "progress")
    if want + credit > ceiling:
        credit = ceiling - want
    return credit > 0


@check_of("prod_keep")
def _c_prodkeep(ev, t):
    return _r_prodkeep(ev, t) == (f(ev, "credit") > 0)


@rule("endgame_unit", ["deep-endgame-unit"], ["endgame_unfinished_gain"],
      note="to = want * endgame_unfinished_gain; "
           "flip = the deflation happens at all")
def _r_endgame(ev, t):
    return t["endgame_unfinished_gain"] != 1.0


# ---- R8: the garrison floor (deepdefence.c deep_apply_garrison_floor)
# applied = (unsigned)(garrison_floor_danger * updisc * menace * w * fmult)
# and eligibility is `units < garrison_floor`.  Every factor is logged.
# The class weights enter through w = max(wbarb, wsea, wair, wborder),
# each of which is its own knob times a per-city geometry factor, so a
# logged nonzero component rescales exactly.

_FLOORW = {"floor_barb_weight": "wbarb", "floor_sea_weight": "wsea",
           "floor_air_weight": "wair", "floor_border_weight": "wborder"}


@rule("garrison_floor", ["deep-floor"],
      ["garrison_floor_danger", "garrison_floor"] + list(_FLOORW),
      note="applied = int(garrison_floor_danger*updisc*menace*w*fmult); "
           "flip = the city gets standing danger at all")
def _r_floor(ev, t):
    if f(ev, "units") >= t["garrison_floor"]:
        return 0                          # not eligible: no floor danger
    comps = {}
    for knob, field in _FLOORW.items():
        base = f(ev, field)
        comps[knob] = base
    # rescale each component by its own knob's ratio to the value the
    # line was written under; a zero component carries no knob signal
    # and is left alone (reported as such in the coverage note).
    w = 0.0
    for knob, base in comps.items():
        w = max(w, base * t["_ratio_" + knob])
    applied = int(t["garrison_floor_danger"] * f(ev, "updisc")
                  * f(ev, "menace") * w * f(ev, "fmult"))
    return applied


@check_of("garrison_floor")
def _c_floor(ev, t):
    return _r_floor(ev, t) == int(f(ev, "applied"))


# ---- R12: the emergency cap's own seam (deepwant.c) -----------------
# deep-milcap is emitted for every EMERGENCY-tier military choice that
# reaches the merge, cap on or off, so the population the cap acts on
# is finally observable.  What this rule reports is a BINDING rate, not
# a decision flip: the cap changes the value that enters the debit
# chain, and the merge itself is tier-first (the 2026-08-30 screen's
# finding, which this rule now measures at the seam instead of
# inferring at the merge).  Read it as "how often would a cap one sigma
# from here touch anything at all"; a nonzero rate is a necessary
# condition for the knob to matter and nothing more.

@rule("emergency_cap", ["deep-milcap"], ["emergency_value_cap"],
      note="BINDING rate, not a decision flip: cap > 0 and pre > cap")
def _r_milcap(ev, t):
    cap = t["emergency_value_cap"]
    return cap > 0 and f(ev, "pre") > cap


@check_of("emergency_cap")
def _c_milcap(ev, t):
    return _r_milcap(ev, t) == (f(ev, "bind") > 0)


# ---- R9: the founder pool (deepsettler.c) ---------------------------
# `founders <= (int)(founder_pool_per_site * sites)` refuses the
# dissolve.  founders and sites are both logged.

@rule("founder_pool", ["deep-pool"], ["founder_pool_per_site"],
      note="dissolve refused while founders <= "
           "int(founder_pool_per_site * remaining sites)")
def _r_pool(ev, t):
    return f(ev, "founders") <= int(t["founder_pool_per_site"]
                                    * f(ev, "sites"))


@check_of("founder_pool")
def _c_pool(ev, t):
    return _r_pool(ev, t) == (f(ev, "founders") <= int(f(ev, "target")))


# ---- R10: the peace bar (deepdiplomacy.c deep_peace_wanted) ---------
# bar = (int)(req_love_for_peace * (1 - peace_seek_gain)).  The knob is
# recoverable because both love and bar are logged and the unlogged
# req_love_for_peace divides out.

@rule("peace_bar", ["deep-peaceseek", "deep-peacetake"],
      ["peace_seek_gain"],
      note="love >= bar, bar = req_love * (1 - peace_seek_gain)")
def _r_peace(ev, t):
    why = ev.get("why")
    if why is not None and why not in ("yes", "love"):
        return None                       # refused for a reason upstream
    g = t["peace_seek_gain"]
    if g >= 1.0:
        return None
    req = f(ev, "bar") / (1.0 - t["_base_peace_seek_gain"]) \
        if t["_base_peace_seek_gain"] < 1.0 else 0.0
    return f(ev, "love") >= int(req * (1.0 - g))


@check_of("peace_bar")
def _c_peace(ev, t):
    r = _r_peace(ev, t)
    return None if r is None else r == (f(ev, "love") >= f(ev, "bar"))


# --------------------------------------------------------------------
# Corpus scan
# --------------------------------------------------------------------

def open_log(path):
    if str(path).endswith(".zst"):
        p = subprocess.Popen(["zstdcat", str(path)], stdout=subprocess.PIPE)
        return p.stdout, p
    return open(path, "rb"), None


def parse_fields(rest):
    out = {}
    for m in FIELD.finditer(rest):
        k = m.group(1).decode()
        v = m.group(2)
        if v.startswith(b'"'):
            out[k] = v[1:-1].decode()
        else:
            try:
                out[k] = float(v)
            except ValueError:
                out[k] = v.decode()
    return out


def build_probes():
    """knobcensus's own instrument probes, tag -> [(knob, pred), ...].

    A `partial` knob cannot be flip-screened, but the rate at which its
    instrument FIRES is the honest second-best signal for a designed
    screen, and it is already defined and already validated in
    knobcensus.py.  Reused rather than re-derived."""
    by_tag = collections.defaultdict(list)
    if compile_pred is None:
        return by_tag
    for knob, ent in CENSUS_MAP.items():
        for tg, pred in (ent[3] or []):
            by_tag[tg].append((knob, compile_pred(pred)))
    return by_tag


def wanted_tags():
    tags = set()
    for r in RULES.values():
        tags.update(r["tags"])
    tags.add("deep-tuning")
    return tags


def base_values(tuning_line, knobs):
    """The knob values THIS game ran under, from its deep-tuning line."""
    base = {k: kb.default for k, kb in knobs.items()}
    if tuning_line:
        for k, v in tuning_line.items():
            if k in base and isinstance(v, float):
                base[k] = v
    return base


def scan_game(path, only_rule):
    """One game log -> (tag counter, tuning dict, per-rule event lists)."""
    tags = collections.Counter()
    fires = collections.Counter()
    fields = collections.Counter()
    tuning = None
    want = wanted_tags()
    probes = build_probes()
    joins = collections.defaultdict(
        lambda: collections.defaultdict(dict))  # rulename -> key -> {tag: ev}
    single = collections.defaultdict(list)     # rulename -> [ev]
    bytag = collections.defaultdict(list)
    for name, r in RULES.items():
        if only_rule and name != only_rule:
            continue
        for tg in r["tags"]:
            bytag[tg].append(r)

    fh, proc = open_log(path)
    try:
        for raw in fh:
            if b"deep-" not in raw:
                continue
            m = LINE.search(raw)
            if m is None:
                continue
            tag = m.group(1).decode()
            tags[tag] += 1
            pr = probes.get(tag)
            if pr:
                rest = m.group(2)
                for knob, pred in pr:
                    if pred(rest):
                        fires[knob] += 1
            if tag not in want:
                continue
            if tag == "deep-tuning":
                if tuning is None:
                    tuning = parse_fields(m.group(2))
                continue
            rules = bytag.get(tag)
            if not rules:
                continue
            ev = parse_fields(m.group(2))
            for (tg, fl) in NEW_FIELDS:
                if tg == tag and fl in ev:
                    fields[(tg, fl)] += 1
            for r in rules:
                if r["join"] is None:
                    single[r["name"]].append(ev)
                else:
                    slot = joins[r["name"]][r["join"](ev)]
                    if tag in slot:
                        if isinstance(slot[tag], list):
                            slot[tag].append(ev)
                        else:
                            slot[tag] = [slot[tag], ev]
                    else:
                        slot[tag] = ev
    finally:
        fh.close()
        if proc is not None:
            proc.wait()
    for name, byk in joins.items():
        single[name] = list(byk.values())
    return tags, tuning, single, fires, fields


def screen_game(path, knobs, only_rule):
    """-> (tags, flips, touched, agree, checked, fires, new fields)"""
    tags, tuning, events, fires, fields = scan_game(path, only_rule)
    base = base_values(tuning, knobs)
    flips = collections.Counter()
    touched = collections.Counter()
    agree = collections.Counter()
    checked = collections.Counter()

    t0 = dict(base)
    t0["_base"] = base
    t0["_base_peace_seek_gain"] = base.get("peace_seek_gain", 0.0)
    for kn in _FLOORW:
        t0["_ratio_" + kn] = 1.0

    for rname, evs in events.items():
        r = RULES[rname]
        decide, chk = r["decide"], r["check"]
        for ev in evs:
            d0 = decide(ev, t0)
            if d0 is None:
                continue
            touched[rname] += 1
            if chk is not None:
                cv = chk(ev, t0)
                if cv is not None:
                    checked[rname] += 1
                    if cv:
                        agree[rname] += 1
            for kname in r["knobs"]:
                kb = knobs.get(kname)
                if kb is None:
                    continue
                for label, v in kb.steps(base[kname]):
                    t1 = dict(t0)
                    t1[kname] = v
                    if kname in _FLOORW:
                        b = base[kname]
                        t1["_ratio_" + kname] = (v / b) if b else float("inf")
                    if decide(ev, t1) != d0:
                        flips[(kname, rname, label)] += 1
    return tags, flips, touched, agree, checked, fires, fields


def collect_logs(roots, seeds_cap):
    out = []
    per_root = collections.Counter()
    for root in roots:
        rp = Path(root)
        for log in sorted(list(rp.rglob("game.log.zst")) + list(rp.rglob("game.log"))):
            if seeds_cap:
                per_root[root] += 1
                if per_root[root] > seeds_cap:
                    continue
            out.append(log)
    return out


_KNOBS = None
_ONLY = None


def _init(knobs, only_rule):
    global _KNOBS, _ONLY
    _KNOBS, _ONLY = knobs, only_rule


def _work(path):
    return screen_game(path, _KNOBS, _ONLY)


# --------------------------------------------------------------------
# Classification of every knob the rules do not reach
# --------------------------------------------------------------------

def classify(knob, knobs, rule_knobs, tags_seen, touched):
    kb = knobs[knob]
    if knob in rule_knobs:
        # A rule whose decision never occurred in this corpus proves
        # nothing about the knob; it is the mechanism that is missing,
        # not the sensitivity.
        if any(touched.get(rn, 0) for rn in rule_knobs[knob]):
            return "reconstructible"
        return "dead-path"
    ent = CENSUS_MAP.get(knob)
    if ent is None:
        return "unmapped"
    kind, grade, probes = ent[1], ent[2], ent[3]
    if probes is None:
        return "not-logged"
    if not any(tags_seen.get(t) for t, _p in probes):
        return "dead-path"
    if kind == "mode" or kb.is_mode():
        return "mode"
    return "partial"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--md", action="store_true")
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--seeds", type=int, default=0)
    ap.add_argument("--rule")
    ap.add_argument("--validate", action="store_true")
    ap.add_argument("--top", type=int, default=0)
    args = ap.parse_args()

    knobs, order = load_knobs()
    logs = collect_logs(args.dirs, args.seeds)
    if not logs:
        sys.exit("no game logs under " + " ".join(args.dirs))

    if args.jobs > 1:
        import multiprocessing as mp
        ctx = mp.get_context("fork")
        with ctx.Pool(args.jobs, initializer=_init,
                      initargs=(knobs, args.rule)) as pool:
            results = pool.map(_work, logs, chunksize=1)
    else:
        _init(knobs, args.rule)
        results = [_work(p) for p in logs]

    tags_seen = collections.Counter()
    flips = collections.Counter()
    touched = collections.Counter()
    agree = collections.Counter()
    checked = collections.Counter()
    fires = collections.Counter()
    fields = collections.Counter()
    for tg, fl, tc, ag, ck, fr, fd in results:
        tags_seen.update(tg)
        flips.update(fl)
        touched.update(tc)
        agree.update(ag)
        checked.update(ck)
        fires.update(fr)
        fields.update(fd)
    n = len(logs)
    unmeasured = {}
    for (tg, fl), kns in sorted(NEW_FIELDS.items()):
        if not fields.get((tg, fl)):
            for k in kns:
                unmeasured.setdefault(k, []).append(f"{tg}.{fl}")

    rule_knobs = {}
    for rname, r in RULES.items():
        if args.rule and rname != args.rule:
            continue
        for k in r["knobs"]:
            rule_knobs.setdefault(k, []).append(rname)

    if args.validate:
        print("REPLAY VALIDATION -- base-value replay vs the logged outcome")
        print(f"{'rule':20s} {'checked':>9s} {'agree':>9s} {'rate':>7s}")
        for rname in sorted(RULES):
            c = checked.get(rname, 0)
            if not c:
                continue
            print(f"{rname:20s} {c:9d} {agree.get(rname,0):9d} "
                  f"{agree.get(rname,0)/c:7.1%}")
        print()

    # ---------------- per-knob table ----------------
    rows = []
    for kname in order:
        kb = knobs[kname]
        cls = classify(kname, knobs, rule_knobs, tags_seen, touched)
        rnames = rule_knobs.get(kname, [])
        tot_touch = sum(touched.get(rn, 0) for rn in rnames)
        best = 0
        detail = []
        for rn in rnames:
            for label in ("-1s", "+1s", "alt"):
                c = flips.get((kname, rn, label), 0)
                if c:
                    detail.append(f"{rn}{label}={c}")
                    best = max(best, c)
        rows.append((kname, cls, kb.default, kb.sigma, tot_touch, best,
                     ",".join(rnames), "; ".join(detail),
                     fires.get(kname, 0)))

    counts = collections.Counter(r[1] for r in rows)
    live = [r for r in rows if r[1] == "reconstructible"]
    movers = sorted([r for r in live if r[5] > 0], key=lambda r: -r[5])
    inert = [r for r in live if r[5] == 0]

    if args.md:
        print("| knob | class | default | sigma | decisions/game | "
              "flips/game | fires/game | rule |")
        print("|---|---|---|---|---|---|---|---|")
        for (k, cls, d, s, tc, best, rn, det, fr) in rows:
            if args.top and best == 0:
                continue
            dec = f"{tc/n:.1f}" if rn else "--"
            fl = f"{best/n:.2f}" if rn else "--"
            print(f"| `{k}` | {cls} | {d:g} | {s:.4g} | {dec} | {fl} | "
                  f"{fr/n:.1f} | {rn or '--'} |")
        print()
    else:
        w = max(len(r[0]) for r in rows)
        print(f"{'knob':{w}s} {'class':16s} {'sigma':>9s} {'dec/game':>9s} "
              f"{'flips/game':>10s} {'fires/game':>10s}  detail")
        for (k, cls, d, s, tc, best, rn, det, fr) in rows:
            print(f"{k:{w}s} {cls:16s} {s:9.4g} "
                  f"{(tc/n if rn else 0):9.1f} {best/n:10.2f} "
                  f"{fr/n:10.1f}  {det}")
        print()

    if unmeasured:
        print("UNMEASURED on this corpus -- the log field the knob's "
              "replay needs is absent,")
        print("so its flip count below is NOT a finding of insensitivity:")
        for k in sorted(unmeasured):
            print(f"  {k:34s} needs {', '.join(unmeasured[k])}")
        print()

    print(f"corpus: {n} game logs, "
          f"{sum(tags_seen.values())} deep- lines, "
          f"{len(tags_seen)} distinct tags")
    print("classes: " + "  ".join(f"{k}={v}" for k, v in sorted(counts.items())))
    print(f"reconstructible knobs that FLIP something: {len(movers)}")
    print(f"reconstructible knobs that flip NOTHING:   {len(inert)}")
    if movers:
        print("\nSHORTLIST (reconstructible, ranked by flips per game)")
        print(f"{'knob':34s} {'flips/game':>11s} {'decisions/game':>15s}  rule")
        for (k, cls, d, s, tc, best, rn, det, fr) in movers:
            print(f"{k:34s} {best/n:11.2f} {tc/n:15.1f}  {det}")
    if inert:
        print("\nINERT under replay (rule exists, no step flips anything)")
        for (k, cls, d, s, tc, best, rn, det, fr) in inert:
            print(f"  {k:34s} decisions/game={tc/n:.1f}  rules={rn}")

    partial = sorted([r for r in rows if r[1] == "partial" and r[8] > 0],
                     key=lambda r: -r[8])
    if partial:
        print("\nTIER B -- partial: the instrument fires but the log does "
              "not carry both sides of the comparison")
        print(f"{'knob':34s} {'fires/game':>11s}")
        for r in partial:
            print(f"{r[0]:34s} {r[8]/n:11.1f}")
        cold = [r[0] for r in rows if r[1] == "partial" and r[8] == 0]
        if cold:
            print(f"  partial knobs whose instrument NEVER fired "
                  f"({len(cold)}): " + ", ".join(cold))

    dead = [r[0] for r in rows if r[1] == "dead-path"]
    if dead:
        print(f"\nNEVER ON A LOGGED PATH IN THIS CORPUS ({len(dead)}) -- "
              "the instrument exists and its tag never appeared")
        for k in dead:
            print(f"  {k}")


if __name__ == "__main__":
    main()
