# Knob-coverage census — campaign v4 gate 7

Built 2026-08-05 on `epic/w03-horizon-content`. Read-only: no build, no
games, no `ai/` change. Tool: `benchmark/knobcensus.py`.

CAMPAIGN-v4 §4 asks, for every one of the 121 keys in
`benchmark/tuning-params.ini`: **which log line or execution counter
evidences this knob's mechanism executing, and on which training cells
does it fire?** A knob with zero population on every training cell is
EXCLUDED from the search rather than searched dead (receipts: "four
changes measured as no-effect while never executing"; tech-wall 0
executions in 70 games; convoy knobs inert pending invasion wants).

Reproduce:

```sh
python3 benchmark/knobcensus.py benchmark/results \
    /tmp/claude-1000/ab-on /tmp/claude-1000/ab-off /tmp/claude-1000/ab-on7 \
    /tmp/claude-1000/sg-on /tmp/claude-1000/bi-pre /tmp/claude-1000/bi-new \
    /tmp/claude-1000/r2size /tmp/claude-1000/r2size2 /tmp/claude-1000/r2probe \
    --jobs 4 --md
python3 benchmark/knobcensus.py RESULTS_DIR... --dirs-only   # provenance
```

The knob → instrument map lives in the script (`MAP`), one entry per
key, each citing the `ai/deep` line it was read off. The script refuses
to run if the map and `tuning-params.ini` disagree, so the map cannot
silently rot when a knob is added.

---

## 1. Headline

Full corpus, 6135 game logs across 32 result-dir cells (the provenance
table in §2 was taken minutes earlier and totals 6129: a concurrent
session was writing new result dirs while this ran):

| verdict | n | meaning |
|---|---|---|
| **LIVE** | 54 | instrument fired on ≥ 1 cell |
| **DEAD** | 1 | instrument readable in the corpus, fired nowhere |
| **NO-CORPUS** | 5 | instrument exists in code, its tag appears in **no log at all** — either never executed, or the tag is newer than every archived binary |
| **NO-INSTRUMENT** | 53 | nothing at `LOG_NORMAL` evidences the knob's arithmetic; cannot be censused until instrumented |
| **MODE** | 8 | mode switch / degenerate range: classified, not censused |

Of the 54 LIVE, **36 are `direct`** (the line fires only on the branch
the knob gates, or a logged field is a monotone function of the knob)
and **18 are `proxy`** (the line proves the enclosing mechanism ran; the
knob's own term is not separable from it).

By campaign block (§3 stage numbering):

| block | knobs | LIVE | DEAD | NO-CORPUS | NO-INSTRUMENT | MODE |
|---|---|---|---|---|---|---|
| S1 ECON | 38 | 10 | 0 | 2 | 22 | 4 |
| S2 MIL | 42 | 20 | 1 | 3 | 18 | 0 |
| S3 SITE/EXPANSION | 37 | 22 | 0 | 0 | 13 | 2 |
| S4 TRADE | 4 | 2 | 0 | 0 | 0 | 2 |

**The census is NOT clean.** Gate 7 as written ("every searched knob
fires on ≥ 1 training cell") cannot be evaluated for 53 of 121 knobs,
because they have no instrument to fire. That is a census category, not
a failure — but it is a launch-blocking one for the blocks that carry
the uninstrumented mass (§4).

---

## 2. The corpus and what it can and cannot say

| cell | games | with metric lines | distinct binaries | tuning files |
|---|---|---|---|---|
| baseline | 5 | 0 | - | - |
| cand0 | 8 | 8 | - | - |
| cand1 | 8 | 8 | - | - |
| cfg0 | 80 | 80 | - | - |
| cfg1 | 37 | 37 | - | - |
| contested | 1670 | 706 | 37 | 21 (incl. none) |
| contested-classic | 60 | 60 | 1 | 1, none absent |
| cramped | 316 | 296 | 5 | 8 (incl. none) |
| cramped-classic | 90 | 90 | 2 | 2 (incl. none) |
| demo | 14 | 4 | - | - |
| garden | 120 | 40 | 5 | none only |
| garden-classic | 40 | 20 | 2 | none only |
| garden2 | 372 | 372 | 3 | 7 (incl. none) |
| garden2-classic | 90 | 90 | 2 | 2 (incl. none) |
| garden2b | 288 | 288 | 3 | 7 (incl. none) |
| gen0 | 24 | 24 | - | - |
| gen1 | 24 | 24 | - | - |
| hutfree | 20 | 20 | 2 | none only |
| huts | 20 | 20 | 2 | none only |
| isolated | 158 | 118 | 6 | 3 (incl. none) |
| isolated-150 | 1249 | 1244 | 29 | 25 (incl. none) |
| isolated-classic | 30 | 30 | 1 | 1, none absent |
| lategame | 60 | 30 | 3 | none only |
| lategame-80 | 263 | 262 | 10 | 9 (incl. none) |
| long | 158 | 138 | 4 | 5 (incl. none) |
| long-classic | 30 | 30 | 1 | 1, none absent |
| primary | 236 | 24 | 1 | 13 (incl. none) |
| primary-120 | 3 | 3 | 2 | 2 (incl. none) |
| primary-120-classic | 64 | 64 | 2 | 3, none absent |
| primary-classic | 24 | 24 | 1 | 1, none absent |
| primary-island | 6 | 6 | - | - |
| smoke | 562 | 332 | 29 | 5 (incl. none) |

Scratch dirs used, and their binaries:

| dir | cell | server sha (12) | tuning |
|---|---|---|---|
| `ab-on`, `ab-off`, `ab-on7` | primary-120-classic | `ac74b9367d3a` | `on.ini` / `off.ini` / `on7.ini` (R2b caravan A/B) |
| `r2size2` | primary-120-classic | `ac74b9367d3a` | `on.ini` |
| `r2probe` | isolated-150 | `96b09440f803` | `on.ini` |
| `r2size` | primary-120 | (no `key.txt`) | -- |
| `sg-on`, `bi-new` | smoke | `ac74b9367d3a` | `on.ini` / `off.ini` |
| `bi-pre` | smoke | `305e3eed09b8` | `off.ini` |

Four things about this corpus that the script enforces rather than
assumes, and that every row inherits:

1. **`FREECIV_DEEP_QUIET` erases the instruments.** `deep_metric_log_level()`
   returns `LOG_VERBOSE` when it is set (`ai/deep/deeptuning.c:360`), and
   `campaign.sh` exports it. 1637 of 6129 logs carry no metric lines at all (that count also includes non-deep
   arms such as `base`). Those games are
   excluded from the denominator, never counted as zeros. **A campaign
   run under `DEEP_QUIET` cannot be censused at all** — §6's
   "execution counters logged per candidate" requires `DEEP_QUIET`
   unset for at least the counter-bearing arm.
2. **A cell whose logs never carry a knob's tag votes `n/a`, not zero.**
   The archived binaries span 2026-07 to 2026-08-05 and many predate the
   instrument being censused. Only cells that show the tag somewhere are
   allowed to vote DEAD; that is what the `LIVE 6/11` style ratio in the
   table means — 6 cells fired, out of 11 whose binary could have.
3. **Results from stale binaries are evidence that an instrument
   EXISTS, and weaker evidence of liveness today.** Demonstrated, not
   argued: re-running the census over only the dirs written since
   2026-08-03 (1661 logs, 5 cells) flips `explore_discovery_value` from
   LIVE-on-29-cells to **DEAD** — `deepexplore.c:180` now zeroes
   `unknown` for any player without the `H_MAP` handicap, which is every
   difficulty actually played. Every other verdict is unchanged between
   the two slices. Treat single-cell and low-ratio LIVE rows as
   provisional until the freeze rerun.
4. **Nine of the 32 "cells" are not venues.** `cand0/cand1/cfg0/cfg1/
   gen0/gen1/baseline/demo/primary-island` come from `tune.py` and probe
   dirs whose directory layout puts a candidate where a venue normally
   sits. They only ever add evidence (a knob is LIVE if any cell fires),
   but they inflate the `readable` denominator. The venue cells are the
   other 23.

Some cells were run with a non-default tuning file (column 5). Those
rows are evidence the instrument works; they are **not** evidence of
liveness at compiled defaults. Two rows depend on it entirely and are
called out in §5.

---

## 3. The full table

`block` is the CAMPAIGN-v4 §3 stage. `cells firing` reads
`fired/readable: cell (lines)`.

| knob | block | instrument | grade | cells firing | verdict |
|---|---|---|---|---|---|
| `settler_fearfulness_mult` | S3 | `deep-settler` | proxy | 13/13: isolated-150 (1744498), lategame-80 (201348), contested (136755), primary-120-classic (127357), smoke (76889), +8 more | **LIVE** |
| `settler_risk_tolerance` | S3 | `deep-settler dangerdest>0 \| deep-settler blocker=danger` | direct | 6/13: primary-120-classic (1574), isolated-150 (1490), contested (419), lategame-80 (371), smoke (76), +1 more | **LIVE** |
| `settler_guard_tolerance` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_enough_result` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `founder_pool_per_site` | S3 | `deep-pool` | direct | 6/6: isolated-150 (150126), lategame-80 (19853), contested (4824), smoke (1290), primary-120-classic (554), +1 more | **LIVE** |
| `committed_fear_mult` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `incoming_defence_eta_weight` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `incoming_defence_survival` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `embassy_science_value` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `explore_hut_value` | S1 | `deep-explore huts_vis>0` | direct | 30/30: isolated-150 (144228), isolated (33858), contested (31646), garden2b (29394), cramped (18683), +25 more | **LIVE** |
| `explore_discovery_value` | S1 | `deep-explore unknown>0` | direct | 29/30: contested (387543), cramped (127245), garden2 (105207), isolated (101370), garden2b (88671), +24 more | **LIVE** |
| `cm_happy_factor` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_war_food` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_war_shield` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_war_trade` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_cap_food` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_cap_shield` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_cap_trade` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_wonder_shield` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_wonder_trade` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_grow_below` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_grow_safety_margin` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `sortie_defence_weight` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_grow_food` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_grow_shield` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_grow_trade` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `cm_trade_quality_slope` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `diplomat_defence_value` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_stream_weight` | S3 | `deep-settler flag:pick` | proxy | 13/13: isolated-150 (113211), contested (20327), lategame-80 (16231), primary-120-classic (10253), isolated (8077), +8 more | **LIVE** |
| `land_pressure_tiles` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_potential_discount` | S3 | `deep-settler flag:pick` | proxy | 13/13: isolated-150 (113211), contested (20327), lategame-80 (16231), primary-120-classic (10253), isolated (8077), +8 more | **LIVE** |
| `site_special_bonus` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_special_cluster` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_claim_bonus` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_pressure_factor` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `horizon_capacity_weight` | S1 | `deep-capacity credit>0` | direct | 6/6: lategame-80 (5571), contested (3688), smoke (253), primary-120-classic (219), isolated-150 (205), +1 more | **LIVE** |
| `horizon_growth_weight` | S1 | `deep-credit` | proxy | 6/6: primary-120-classic (453472), contested (155258), lategame-80 (141878), smoke (107498), primary-120 (10930), +1 more | **LIVE** |
| `horizon_bundle_weight` | S1 | `deep-credit` | proxy | 6/6: primary-120-classic (453472), contested (155258), lategame-80 (141878), smoke (107498), primary-120 (10930), +1 more | **LIVE** |
| `horizon_health_weight` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `horizon_content_weight` | S1 | `deep-credit` | proxy | 6/6: primary-120-classic (453472), contested (155258), lategame-80 (141878), smoke (107498), primary-120 (10930), +1 more | **LIVE** |
| `horizon_multiplier_weight` | S1 | `deep-credit` | proxy | 6/6: primary-120-classic (453472), contested (155258), lategame-80 (141878), smoke (107498), primary-120 (10930), +1 more | **LIVE** |
| `site_travel_discount` | S3 | `deep-settler flag:pick` | proxy | 13/13: isolated-150 (113211), contested (20327), lategame-80 (16231), primary-120-classic (10253), isolated (8077), +8 more | **LIVE** |
| `site_travel_crowding` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_potential_weight` | S3 | `deep-settler flag:pick` | proxy | 13/13: isolated-150 (113211), contested (20327), lategame-80 (16231), primary-120-classic (10253), isolated (8077), +8 more | **LIVE** |
| `site_settler_cost_weight` | S3 | `deep-settler flag:pick` | proxy | 13/13: isolated-150 (113211), contested (20327), lategame-80 (16231), primary-120-classic (10253), isolated (8077), +8 more | **LIVE** |
| `site_boat_cost_weight` | S3 | `deep-settler overseas>0 \| deep-settler virt>0` | direct | 13/13: isolated-150 (63730), contested (12290), isolated (7618), long (6478), lategame (4943), +8 more | **LIVE** |
| `site_rival_pressure` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_rival_crowding` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `expansion_war_survival` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `caravan_discount` | S4 | `deep-trade has:discount` | direct | 5/5: isolated-150 (10107), lategame-80 (1183), primary-120-classic (1048), contested (47), smoke (21) | **LIVE** |
| `caravan_foreign_trade` | S4 | `deep-trade has:reach` | direct | 5/5: isolated-150 (10107), lategame-80 (1183), primary-120-classic (1048), contested (47), smoke (21) | **MODE** |
| `cm_war_survival` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `horizon_tech_weight` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `overseas_fear` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `overseas_decay` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `garrison_floor` | S2 | `deep-sat \| deep-rotate` | proxy | 13/13: isolated-150 (3775247), contested (444173), lategame-80 (426278), primary-120-classic (422890), long (250441), +8 more | **LIVE** |
| `garrison_floor_danger` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `garrison_menace_slope` | S2 | `deep-def menace>0` | direct | 13/31: isolated-150 (2450777), contested (857428), lategame-80 (568845), primary-120-classic (490028), garden2b (477625), +8 more | **LIVE** |
| `menace_overseas_weight` | S2 | `deep-def menace>0` | proxy | 13/31: isolated-150 (2450777), contested (857428), lategame-80 (568845), primary-120-classic (490028), garden2b (477625), +8 more | **LIVE** |
| `barbarian_fear` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `boat_shadow_fear` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `density_fear_value` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `density_fear_radius` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `density_fear_ramp_turns` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `density_prior_value` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `density_prior_tiles` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `wall_fear_weight` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `settler_rush_weight` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `site_choke_bonus` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `convoy_min_load` | S2 | `deep-convoy flag:hold` | direct | -- | **NO-CORPUS** |
| `convoy_wait_radius` | S2 | `deep-convoy flag:hold` | proxy | -- | **NO-CORPUS** |
| `worker_flee_radius` | S3 | `deep-flee` | direct | 13/13: contested (92709), lategame-80 (91964), garden2b (36345), primary-120-classic (26531), lategame (21804), +8 more | **LIVE** |
| `worker_guard_radius` | S3 | `deep-guard` | direct | 13/13: lategame-80 (30606), contested (21176), isolated-150 (7473), smoke (3144), lategame (2628), +8 more | **LIVE** |
| `worker_danger_discount` | S3 | `-` | - | -- | **NO-INSTRUMENT** |
| `worker_cross_continent_discount` | S3 | `deep-idlework cross>0` | direct | 4/6: isolated-150 (7421), lategame-80 (1375), contested (525), smoke (74) | **LIVE** |
| `upkeep_gold_weight` | S1 | `deep-credit` | proxy | 6/6: primary-120-classic (453472), contested (155258), lategame-80 (141878), smoke (107498), primary-120 (10930), +1 more | **LIVE** |
| `upkeep_unit_weight` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `rotation_power_ratio` | S2 | `deep-rotate` | direct | 12/12: isolated-150 (3094), garden2b (1752), contested (559), isolated (518), primary-120-classic (517), +7 more | **LIVE** |
| `rotation_want` | S2 | `-` | - | -- | **NO-INSTRUMENT** |
| `rotation_finish_relax` | S2 | `deep-rotate has:disband` | proxy | 12/12: isolated-150 (2590), garden2b (1300), contested (517), isolated (489), long (434), +7 more | **LIVE** |
| `survival_target` | S2 | `deep-surv \| deep-meltblock` | direct | 11/11: contested (223362), lategame-80 (165394), primary-120-classic (76031), lategame (73857), isolated-150 (73298), +6 more | **LIVE** |
| `survival_fear_value` | S2 | `deep-surv fear>0` | direct | 11/11: lategame-80 (92704), contested (71183), lategame (40616), primary-120-classic (31096), isolated-150 (27218), +6 more | **LIVE** |
| `survival_horizon` | S2 | `deep-surv` | direct | 11/11: contested (223355), lategame-80 (165392), primary-120-classic (76031), lategame (73857), isolated-150 (73298), +6 more | **LIVE** |
| `tired_attack_floor` | S2 | `deep-tired` | direct | -- | **NO-CORPUS** |
| `attack_value_weight` | S2 | `deep-kill` | direct | 11/11: contested (661239), isolated-150 (593100), primary-120-classic (345990), lategame-80 (274706), long (231390), +6 more | **LIVE** |
| `sortie_win_floor` | S2 | `deep-sortie has:kill` | direct | 10/11: lategame-80 (627), contested (564), isolated-150 (518), lategame (156), primary-120-classic (131), +5 more | **LIVE** |
| `op_take_target` | S2 | `deep-kill \| deep-op flag:retarget` | direct | 11/11: contested (661516), isolated-150 (610342), primary-120-classic (346310), lategame-80 (283327), long (231866), +6 more | **LIVE** |
| `op_muster_wait` | S2 | `deep-op flag:abort-wait \| deep-op flag:abort-march \| deep-op flag:strike \| deep-op flag:garrison` | direct | 9/11: contested (2763), isolated-150 (2625), lategame-80 (2472), isolated (406), primary-120-classic (384), +4 more | **LIVE** |
| `satisfied_fear_discount` | S2 | `deep-sat` | direct | 11/11: isolated-150 (3772153), contested (443614), lategame-80 (426197), primary-120-classic (422373), long (249989), +6 more | **LIVE** |
| `colony_lift_weight` | S3 | `deep-colony has:lift` | direct | 11/11: isolated-150 (279015), lategame-80 (181873), lategame (67075), long (15076), contested (13518), +6 more | **LIVE** |
| `invasion_reinforce_rate` | S2 | `deep-invade` | direct | 6/6: isolated-150 (14739), lategame-80 (3628), long (257), contested (209), lategame (170), +1 more | **LIVE** |
| `op_wave_window` | S2 | `deep-op wave2>0` | direct | 8/11: contested (244), lategame (37), primary-120-classic (35), lategame-80 (14), smoke (13), +3 more | **LIVE** |
| `raid_radius` | S2 | `deep-raid \| deep-op flag:raid` | direct | 10/11: contested (32329), long (10213), lategame-80 (7731), isolated-150 (6253), lategame (4282), +5 more | **LIVE** |
| `hut_risk_radius` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `hut_risk_discount` | S1 | `-` | - | -- | **NO-INSTRUMENT** |
| `op_abort_floor` | S2 | `deep-op flag:abort-strike \| deep-op flag:abort-odds` | direct | 9/11: isolated-150 (1964), lategame-80 (722), contested (683), primary-120-classic (425), lategame (354), +4 more | **LIVE** |
| `barb_reinforce_rate` | S2 | `deep-kill barb>0` | proxy | 1/11: contested (209) | **LIVE** |
| `lift_form_radius` | S3 | `deep-lift-op flag:form` | direct | 6/6: isolated-150 (27361), lategame-80 (2246), smoke (627), contested (592), primary-120-classic (30), +1 more | **LIVE** |
| `lift_gather_turns` | S3 | `deep-op flag:lift-gather-timeout \| deep-lift-op flag:hitch-wait` | direct | 6/11: isolated-150 (3063), lategame-80 (165), contested (46), smoke (41), primary-120-classic (2), +1 more | **LIVE** |
| `lift_cross_turns` | S3 | `deep-op flag:lift-cross-timeout` | direct | 4/11: isolated-150 (4788), contested (76), lategame-80 (49), smoke (17) | **LIVE** |
| `lift_op_slots` | S3 | `deep-hitch` | proxy | 6/6: isolated-150 (519769), lategame-80 (91273), contested (71471), primary-120-classic (46342), smoke (36800), +1 more | **LIVE** |
| `lift_manifest_max` | S3 | `deep-lift-op flag:hitch` | proxy | 6/6: isolated-150 (3889), lategame-80 (110), contested (20), smoke (20), primary-120 (2), +1 more | **LIVE** |
| `lift_hitch_wait_ratio` | S3 | `deep-lift-op flag:hitch-wait` | direct | 6/6: isolated-150 (2148), lategame-80 (18), smoke (16), primary-120-classic (2), contested (1), +1 more | **LIVE** |
| `boat_idle_damping` | S3 | `deep-hull` | direct | 6/6: isolated-150 (55475), contested (3755), lategame-80 (1075), primary-120-classic (1060), smoke (767), +1 more | **LIVE** |
| `boat_water_gate` | S3 | `deep-watergate` | direct | 3/3: isolated-150 (1691), lategame-80 (227), primary-120-classic (188) | **MODE** |
| `boat_gate_at_bid` | S3 | `deep-watergate-bid \| deep-watergate-attack` | direct | 5/5: isolated-150 (1241), lategame-80 (288), smoke (187), primary-120-classic (140), contested (102) | **MODE** |
| `lift_drop_radius` | S3 | `deep-lift-op flag:nextstop` | direct | 5/6: isolated-150 (648), lategame-80 (10), smoke (2), contested (1), primary-120-classic (1) | **LIVE** |
| `lift_hitch_wait_min` | S3 | `deep-lift-op flag:hitch-wait` | proxy | 6/6: isolated-150 (2148), lategame-80 (18), smoke (16), primary-120-classic (2), contested (1), +1 more | **LIVE** |
| `stance_forced` | S1 | `deep-stance mode=PINNED` | direct | 1/6: isolated-150 (36000) | **MODE** |
| `stance_source` | S1 | `deep-stance has:src` | direct | 6/6: contested (71481), isolated-150 (55200), primary-120-classic (46346), smoke (36800), lategame-80 (28800), +1 more | **MODE** |
| `stance_trait_span` | S1 | `deep-stance has:src` | direct | 6/6: contested (71481), isolated-150 (55200), primary-120-classic (46346), smoke (36800), lategame-80 (28800), +1 more | **MODE** |
| `stance_fear_gain` | S2 | `deep-stance fear_gate>0` | direct | 1/6: isolated-150 (7501) | **LIVE** |
| `stance_op_gain` | S2 | `deep-stance op_gate>0` | direct | 1/6: isolated-150 (3952) | **LIVE** |
| `stance_growth_gain` | S1 | `deep-stance growth_gate>0` | direct | 1/6: isolated-150 (21648) | **LIVE** |
| `stance_expand_gain` | S1 | `deep-stance expand_gate>0` | direct | 1/6: isolated-150 (23440) | **LIVE** |
| `stance_assault_gain` | S2 | `deep-stance assault_gate>0` | direct | 0/6 | **DEAD** |
| `rates_enabled` | S1 | `deep-rates` | direct | -- | **MODE** |
| `rates_science_gain` | S1 | `deep-rates vsci>0` | direct | -- | **NO-CORPUS** |
| `rates_luxury_gain` | S1 | `deep-rates vlux>0` | direct | -- | **NO-CORPUS** |
| `trade_merit` | S4 | `deep-trade-merit` | direct | 4/4: primary-120-classic (161446), isolated-150 (1611), primary-120 (134), smoke (110) | **MODE** |
| `trade_merit_gain` | S4 | `deep-trade-merit merit>0` | direct | 1/4: primary-120-classic (140874) | **LIVE** |

Two rows need reading with §2's caveat 3 and §5 in hand:
`explore_discovery_value` is LIVE here only on binaries older than
2026-08-03 and is DEAD on current ones; `trade_merit_gain` is LIVE only
on dirs that ran `trade_merit = 1` from a tuning file.

---

## 4. NO-INSTRUMENT — 53 knobs that cannot be campaign-searched yet

These are not dead. They are **unobservable**: nothing at `LOG_NORMAL`
tells you whether the knob's term was computed, was nonzero, or changed
a decision. §6's rule "a candidate whose counters read zero for its
changed knobs is flagged, not scored" cannot be applied to any of them,
and §7's fourth stop-trigger ("any candidate's counters go silent on
knobs it changed") is silent by construction.

They fall into five clusters. The instrument each cluster needs is one
change, not 53.

### 4a. The fear lattice — 15 knobs, S2 (the biggest single hole)

`overseas_fear`, `overseas_decay`, `boat_shadow_fear`,
`density_fear_value`, `density_fear_radius`, `density_fear_ramp_turns`,
`density_prior_value`, `density_prior_tiles`, `land_pressure_tiles`,
`barbarian_fear`, `garrison_floor_danger` — eleven on the injection
side — plus `incoming_defence_eta_weight`, `incoming_defence_survival`,
`committed_fear_mult` and `upkeep_unit_weight` on the credit side.

**Cause, one line:** `deep_log_defence()` runs *before*
`deep_credit_incoming_defence()` (`deepdefence.c:1989-1990`), and the
code says so — "`danger` above is the RAW stock figure — deep's injected
fears land after this line". So `deep-def danger=` is stock's number and
every deep fear is invisible.

**Needs:** per-fear fields on `deep-def`, or a second line after the
injection pass carrying `over= shadow= density= barb= floor= credit=
updisc=`. One log line unlocks 15 knobs, i.e. most of the MIL block.

This is why stage 2's fear lattice is currently untunable by §6's rules:
the optimiser can move `density_fear_value` and nothing in the run will
ever say whether any city saw a density fear.

### 4b. The site evaluator's conditional terms — 8 knobs, S3

`site_special_bonus`, `site_special_cluster`, `site_claim_bonus`,
`site_pressure_factor`, `site_rival_pressure`, `site_rival_crowding`,
`site_choke_bonus`, `site_travel_crowding`.

**Cause:** `ai/deep/deepsite.c` emits **no log line at all**. The five
site knobs that ARE censused (`site_stream_weight`,
`site_potential_discount`, `site_potential_weight`,
`site_travel_discount`, `site_settler_cost_weight`) got there only as
`proxy` rows on `deep-settler ... pick`, because they are unconditional
multipliers — if the evaluator ran, they applied. The eight above are
*conditional*: each multiplies a count (`nspecials`, `nclaim`,
`contest`, `pressured`, choke, crowding) that may be structurally zero
in a venue, which is exactly what the census exists to detect.

**Needs:** `nspecials= nclaim= contest= pressured= choke=` on the
`deep-settler ... pick` line. One line change, eight knobs.

### 4c. The city manager — 17 knobs, S1

`cm_happy_factor`, `cm_war_food/shield/trade`, `cm_cap_food/shield/trade`,
`cm_wonder_shield/trade`, `cm_grow_below`, `cm_grow_safety_margin`,
`cm_grow_food/shield/trade`, `cm_trade_quality_slope`,
`cm_war_survival`, `sortie_defence_weight`.

**Cause:** `ai/deep/deepcm.c` emits no log line at all. The CM has four
branches (war / capped / wonder / growing) and nothing says which one a
city took.

**Needs:** one `deep-cm` line per city per management pass with the
branch name and the factor vector actually handed to the CM. That is
the whole ECON block's arrangement half.

Note `cm_war_survival` in particular: `deep-surv` proves the survival
estimator runs, but *not* that `deep_set_city_manager_params()` took its
war branch. The two thresholds are deliberately separate
(`deepcm.c:199`), so one cannot stand in for the other.

### 4d. Build-side wants that never reach a line — 7 knobs

`rotation_want` (the quiet-city rotation bid; `deep-rotate` covers only
the melt/upgrade side), `settler_rush_weight` (applied to `founder_want`
*after* `deep-def` is written), `expansion_war_survival`
(`deep_expansion_blocked()` returns a bool nobody logs),
`diplomat_defence_value`, `embassy_science_value`, `wall_fear_weight`,
`site_enough_result` (the phase-2 skip is a bare `return`).

**Needs:** these are individually cheap — a counter each, or route them
through the existing `deep-want` line, which already reports the winning
build pick and its tier.

### 4e. Horizon terms folded into one aggregate — 3 knobs + 5 proxies

`horizon_health_weight` and `horizon_tech_weight` have no usable
instrument; `hut_risk_radius` and `hut_risk_discount` fold into
`hut_weight` before `deep-explore` prints anything.

`horizon_tech_weight` is a special case worth stating plainly: its
instrument **exists** — `deep-techwall` at `deephorizon.c:850` — but it
is written at `LOG_DEBUG`, so it is absent from every benchmark log ever
taken. The standing receipt "tech-wall: 0 executions in 70 games" is
consistent with both "never fires" and "never printed"; this census
cannot separate them. **Needs:** promote it to
`deep_metric_log_level()`, or add `techwall=` to `deep-credit`.

The five `deep-credit` proxies (`horizon_growth_weight`,
`horizon_bundle_weight`, `horizon_content_weight`,
`horizon_multiplier_weight`, `upkeep_gold_weight`) are LIVE only in the
weak sense that the credit ladder ran. `deep-credit` reports one summed
`add=` per city; the per-term breakdown exists only in the `LOG_DEBUG`
`deep-horizon` line, and even that is per-improvement, not per-term.
**Needs:** a per-term breakdown on `deep-credit`.

### 4f. The rest — 2 knobs

`settler_guard_tolerance` (bodyguard requests logged only via
`UNIT_LOG` at `LOG_DEBUG`), `worker_danger_discount` (divides a task
want silently), `settler_fearfulness_mult` — the last is a `proxy` row,
not NO-INSTRUMENT, but it deserves the same warning: a `deep-settler`
line proves the risk-aware pathfinder ran, and nothing isolates the
multiplier's effect on edge cost.

---

## 5. DEAD and NO-CORPUS — the six knobs that never fired

### `tired_attack_floor` — S2 — **structurally dead: wrong seam**

`deep-tired` appears in **zero** of the 6135 logs, and `deep-convoy`,
its neighbour in the same pass, likewise. The pass itself demonstrably
runs: `deep_melt_surplus_defenders()` (→ `deep-rotate`),
`deep_flee_workers()` (→ `deep-flee`, `deep-guard`) and
`deep_kill_sorties()` (→ `deep-sortie`) are called on the lines either
side of it (`deepdefence.c:1991-1995`) and all three fire in the
hundreds of thousands.

The reason is the seam. `deep_hold_tired_attackers()` is called from
`deep_do_first_activities()`, i.e. **before unit movement**, and its
filter is `punit->moves_left > 0 && punit->moves_left < SINGLE_MOVE`
(`deepdefence.c:1494-1497`). At phase start every unit has its full
move rate, so no unit is ever tired when the pass looks. The guard is
correct code scheduled where its population cannot exist — the same
class of error as the standing method note "log at the seam where the
decision COMMITS".

Note the ruleset gate is NOT the cause: `game.info.tired_attack` is
`TRUE` in civ2civ3 (`data/civ2civ3/game.ruleset:236`) and only `FALSE`
in classic, and the corpus is mostly civ2civ3.

**Campaign action:** EXCLUDE from v4. **Separately:** this is a defect
worth a mechanism epic (move the check to the attack decision), not a
tuning dimension. Not fixed here — this session is read-only.

### `convoy_min_load`, `convoy_wait_radius` — S2 — inert population

`deep-convoy` appears in zero logs. Both branches need a population that
deep's own ferry protocol never produces: the escort branch needs an
idle armed sea unit sharing a tile with a loaded transport in an own
city, and the hold branch needs a **second** passenger that has claimed
the same boat and stands within `convoy_wait_radius`
(`deepdefence.c:1259-1272`). The 1:1 ferry cap means a boat is claimed
by exactly one passenger, who boards and sails inside one manage pass —
the code says as much at `deepdefence.c:1250-1257`.

This confirms the standing receipt ("convoy knobs inert pending invasion
wants") with a mechanism, not a memory. **Campaign action:** EXCLUDE, as
CAMPAIGN-v4's cut-line already anticipates if M2 stays outside the gates
(§2, open question 2).

**CORRECTION (2026-08-12, M2 session).** The verdict EXCLUDE stands; the
mechanism above was the wrong reason, and the receipt it "confirmed" was
wrong twice over.

1. `deep_manage_convoys()` was **dead in its entirety**, not starved of a
   population. Its hull loop tested `utype_move_type()`, which returns
   `UMT_LAND` for every unit class on the server (`set_unit_move_type()`
   is called only from `client/packhand.c`), so the loop rejected every
   boat and neither branch was ever reached. The 1:1-ferry-cap argument
   above is a true statement about the hold branch that never got to
   bind. Fixed unconditionally on `epic/movetype` (merged 2026-08-12);
   `deep-convoy` went 0 → 1 line on the `tech65sea-150` liveness pair.
2. "Pending invasion wants" was already stale when it was written here.
   Invasion wants landed on **2026-07-31** and the defence-at-arrival
   go/no-go closed as task #16 on **2026-08-01** — four days before this
   census. See STATUS "#16 closed: the invade gate at the right seam"
   and `deep_assault_want()`'s overseas branch.

So the family is inert because its own gate was broken, and it is only
now — post-`movetype`, on a naval venue — that a census of it means
anything.

**First honest re-census** (2 games, `tech65sea-150`, deep arm,
compiled defaults, `benchmark/results/m2-liveness/`): `convoy_min_load`
now reads **LIVE, population 1**. One firing in two games is not a
searchable dimension, so **the campaign action EXCLUDE stands** — but
it now rests on a measured population rather than on a mechanism claim
that was false. Re-read it at freeze on a larger naval sample before
deciding otherwise.

### `rates_science_gain`, `rates_luxury_gain` — S1 — off by default

`deep-rates` appears in zero logs, which is exactly right:
`rates_enabled` defaults to `0` and `deep_manage_taxes()` returns
immediately (`deeprates.c:463`). The two gains are inert until the mode
switch is turned on.

**Campaign action:** stage 1 must run its rates arm with
`rates_enabled = 1` **pinned**, and report the mode. Searching the gains
with the switch at its default is searching two dead dimensions. R1's
measured start bounds (`rates_luxury_gain` 0.1–0.25,
`rates_science_gain` 4–13, CAMPAIGN-v4 §3) are unusable otherwise. Note
also that no archived run has ever exercised the rates model — there is
no prior evidence at all about its behaviour in a full game.

### `stance_assault_gain` — S2 — **DEAD, and the corpus cannot say why**

`deep-stance` carries five cumulative gate counters. Four of them fire:

| gate | knob | fired |
|---|---|---|
| 1 fear | `stance_fear_gain` | isolated-150, 7501 |
| 2 op | `stance_op_gain` | isolated-150, 3952 |
| 3 growth | `stance_growth_gain` | isolated-150, 21648 |
| 4 expand | `stance_expand_gain` | isolated-150, 23440 |
| 5 assault | `stance_assault_gain` | **nowhere** |

All four live counters come from one place: the E34 **pinned** runs on
`isolated-150`. Under the standing baseline (FREE, identity axes) every
gate multiplier is exactly 1.0 and none of the counters can increment
(`deepdefence.c:968` and siblings guard on `mult != 1.0`). So all five
stance gains are **pin-conditional by construction**: they are inert in
any unpinned arm, and the only pinned cell in the whole archive is
`isolated-150`.

Gate 5's exemption then explains the zero without needing a bug:
`deep_assault_want()` skips the scaling when
`deep_op_target_threatens(pplayer, target)` (`deepdefence.c:3240`) — a
barbarian holding, or a hostile city on a landmass where we hold cities.
On an isolated map that exemption covers essentially every target the AI
ever assaults.

**Campaign action:** do not exclude yet, and do not call it broken. The
census's honest verdict is *unexercised*, not *inert*: it needs one
pinned-MILITARY cell on a **contested** or **cramped** map before its
counter means anything. That cell does not exist in the archive and
should be added to the separability pilot (§5 of CAMPAIGN-v4), which is
cheap and already pinned.

### `explore_discovery_value` — S1 — dead on current binaries

LIVE on 29 cells of the full archive, **DEAD** on every dir written
since 2026-08-03. `deepexplore.c:179-181` now sets `unknown = 0` for any
player without the `H_MAP` handicap, which is every difficulty actually
played, with the reasoning recorded in the source: deep's own settler
routing runs omniscient, so paying to uncover a map the planner already
reads is billing twice. The knob multiplies a structural zero until W04
makes deep honest.

**Campaign action:** EXCLUDE from v4, with the reason "term disabled
pending W04", not "measured as no-effect".

---

## 6. What this means for the v4 block map

1. **Stage 2 (MIL) cannot be searched as designed.** Its headline
   family — the fear lattice — is 15 knobs with no instrument, and its
   two other candidates (`tired_attack_floor`, the convoy pair) are the
   dead ones. What IS live and direct in S2 is worth searching:
   `survival_target`/`survival_fear_value`/`survival_horizon`,
   `satisfied_fear_discount`, `garrison_menace_slope`,
   `rotation_power_ratio`, `attack_value_weight`, `op_take_target`,
   `op_muster_wait`, `op_abort_floor`, `op_wave_window`, `raid_radius`,
   `sortie_win_floor`, `invasion_reinforce_rate`. **Recommendation:**
   either land the post-injection `deep-def` line before freeze (one
   log line, 15 knobs) or drop the fear lattice from v4 and say so.
2. **Stage 1 (ECON) is more than half unobservable** — 22 of 38, of
   which 17 are the city manager. The CM factors are precisely the
   knobs the ECON block exists to tune. **Recommendation:** a `deep-cm`
   line before freeze, or stage 1 shrinks to the horizon/explore/rates
   knobs and the CM waits for v5.
3. **Stage 3 (SITE/EXPANSION) is the healthiest block** — 22 LIVE of
   37, and the whole lift cluster that NEXT-SESSION §0 owes a tuning
   pass is LIVE and mostly `direct`: `lift_form_radius` 6/6,
   `lift_gather_turns` 6/11, `lift_cross_turns` 4/11,
   `lift_hitch_wait_ratio` 6/6, `lift_drop_radius` 5/6,
   `boat_idle_damping` 6/6, `colony_lift_weight` 11/11,
   `lift_op_slots` 6/6. Every one of them is dominated by
   **isolated-150** (e.g. `lift_cross_turns`: isolated-150 4788,
   contested 76, lategame-80 49, smoke 17). That is the venue-lock
   CAMPAIGN-v4 §4 asks to be pinned into the block map: **tune the lift
   cluster on isolated-150 + lategame-80**; contested and smoke give it
   almost no population. Its eight uninstrumented site terms are the
   block's own gap.
4. **Stage 4 (TRADE) is small and correctly venue-locked.**
   `caravan_discount` fires on 5/5 cells that could read it.
   `trade_merit_gain` fires on **one** cell, `primary-120-classic`
   (140874 lines) — and only because those dirs ran with
   `trade_merit = 1` in `on.ini`. At compiled defaults the merit bid is
   off, and `*-classic` is the only place domestic route revenue is
   nonzero at all (R2). Pin `trade_merit = 1` and the venue to classic,
   or the stage searches nothing.
5. **Five stance gains are pin-conditional.** They are searchable only
   inside a PINNED arm, which stages 1 and 2 are — but every result must
   be labelled with its mode, and any FREE confirm run will read their
   counters as zero *correctly*, not as a defect. Do not let §7's
   fourth stop-trigger fire on them.
6. **18 of the 54 LIVE rows are `proxy` only.** They pass gate 7's
   letter but not its spirit: a nonzero count proves the mechanism ran,
   not that the knob's term was nonzero. The five `deep-credit` horizon
   proxies and the five `deep-settler ... pick` site proxies are the
   bulk. Treat a proxy row's "LIVE" as "not excluded", never as "this
   knob has population".

---

## 7. Rerun at freeze

CAMPAIGN-v4 §4: "the census script joins the campaign artifacts and
reruns at freeze." Concretely, at the frozen commit:

```sh
# one short batch per training cell, DEEP_QUIET UNSET, tuning=none
./benchmark/run.sh --config <cell> --server <pinned> --seeds 5
python3 benchmark/knobcensus.py <those result dirs> --md
```

and require, before launch:

- every cell run on the **frozen binary** (the `--dirs-only` view shows
  one server sha per cell — today `isolated-150` alone shows 29);
- `DEEP_QUIET` unset, so the instruments exist at all;
- `tuning=none` for the census arm, so LIVE means "live at compiled
  defaults" — today two rows (`trade_merit_gain`, and the four firing
  stance gates) are LIVE only because their dirs ran a non-default
  tuning file;
- every training cell of every block present, including a **pinned
  contested/cramped cell** so `stance_assault_gain` can be decided;
- the map re-read against `tuning-params.ini` — the script exits
  non-zero if a knob has been added without an entry.

Then the excluded-dead list goes into the campaign report as CAMPAIGN-v4
§4 requires, and the searched-dimension count that §9's budget sketch is
waiting on is finally a number: **54 LIVE minus 18 proxy-only leaves 36
knobs whose search a counter can actually police**, against a nominal
121.

---

## 8. Status note (not for STATUS.md — D1 owns the next section)

Gate 7 is **built and run, and it is RED**. It does not block on more
measurement; it blocks on four log-line changes (post-injection
`deep-def`, `deep-cm`, site counts on `deep-settler pick`, per-term
`deep-credit`) that between them convert 42 of the 53 NO-INSTRUMENT
knobs into censusable ones (15 + 17 + 8 + 2). That is a mechanism epic, and it is the
cheapest remaining thing standing between the archive and a campaign
whose counters mean what §6 says they mean.
