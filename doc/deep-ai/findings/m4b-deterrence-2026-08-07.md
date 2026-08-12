# M4b — the deterrence seat: pricing what a defender is actually worth

2026-08-07. Session on `epic/w03-horizon-content`, base `6fdcd6cfd9`,
final `9275e9bb6d`. Seven code commits and the docs. `ai/deep/`,
`tests/`, `benchmark/` and doc only — **zero** `ai/default/`,
`ai/baseline/` or `server/advisors/` changes. Every stock WAG line below is cited as evidence and none of
them is edited.

Every mechanism ships at a compiled default of off. Six new knobs, all
PLACEHOLDER, all registered; `tune-audit.py` **CLEAN 202/202/202/202**
(was 196 at session start). `meson test -C build` **4/4 OK**.

    5a7694ea85  substrate: the defender's side of the chained-duel
                ledger (deep_stack_survival_full, deep_city_defence_read)
    41055ee314  item 1: killstack prize in the sortie   sortie_prize_gain
    163b7509ed  item 2: the deterrence dividend         deterrence_gain
    228bddb309  item 5: unit lifetime upkeep at the build
                                                        unit_upkeep_charge
    09202e3416  item 3: walls as pop-loss insurance     pop_insurance_gain
    03620395b4  item 4: the barracks as two streams     heal_stream_gain,
                                                        veteran_stream_gain
    9275e9bb6d  item 5 fix: charge the food half too (§6a)

---

## 0. The one idea

Everything this seat re-prices is the same omission stated five ways:
**deep prices a defender by whether it HOLDS, and never by what it
COSTS the attacker.** The survival deficit fear, the garrison floor,
the counter-claim, the sortie gate — all of them are P(hold) and
nothing else, so a blade that is going to lose is worth zero, a stack
that is going to be annihilated is worth nothing to kill, and a wall
is worth `units × 2`.

The substrate to fix that was already built and half-read. `deepodds`'
chained-duel DP computes the whole attrition distribution; the city
read threw the attacker-loss half away and never had the defender-loss
half at all. Both are one function call, and every item here is a
consumer of one of them.

---

## 1. The substrate: the DP always knew, and never said

`deep_stack_survival_full()` (commit `5a7694ea85`) adds one output to
the chained-duel walk: the expected number of DEFENDERS killed.

It costs nothing. The DP's state index `i` is the front defender,
which is to say defenders `0..i-1` are already dead, so the
mass-weighted mean of `i` at the end of the walk — plus `n_def` for
the mass that reached total annihilation — is the answer. It was
always in the state vector.

**Why it is the interesting number.** In freeciv a defender dies
exactly when an attack on it succeeds. So

    E[defenders killed]  ==  E[successful enemy attacks]

and a successful attack on a city stack costs the city a citizen under
`killcitizen` + `UCF_KILLCITIZEN` (`server/unithand.c:4601`). That is
precisely the quantity `EFT_UNIT_NO_LOSE_POP` insures and that stock
prices as `units × 2` (`daieffects.c:456`). Item 3 is a direct
consumer; nothing else in the project could previously ask the
question.

`deep_city_defence_read()` routes both halves at the city level —
`att_loss` (expected attacker shields destroyed) and `hits` (expected
successful attacks) — with an `extra` list, so the marginal value of
one more defender is *priced* rather than guessed.

Five new checks in `tests/deepattrition_test.c`, every one a closed
form or the accounting identity

    E[attackers dead] + E[defenders dead] = E[duels fought]

which is what makes the new output checkable rather than merely
plausible.

---

## 2. Item 1 — the killstack prize (`sortie_prize_gain`)

`deep_kill_sorties()` had two probability gates and **no target value
at all**: a lone Warriors adjacent and a nine-unit siege stack
adjacent were the same decision at the same odds. That is the one
asymmetry the operations layer does not have — an op values the
conquest it buys.

**Verified mechanic.** `is_stack_vulnerable()` (`common/combat.c:996`)
is FALSE inside a city and on `EF_NO_STACK_DEATH` terrain. A sortie
hits *adjacent* tiles, which are normally neither, so under killstack a
won duel really does annihilate the whole tile and the prize really is
the summed shield value of the stack.

With the gain positive, selection is by

    p × prize × gain  −  (1 − p) × our own build cost

and that score must be positive **on top of** the two existing gates,
which are untouched. At 0 the pass selects by best chance exactly as
before.

Counters: `deep-sortie` gains `prize=`/`score=`; **`deep-sortie-no` is
new and logs the prize the pass DECLINED** — because "turns down big
stacks" and "never sees a big stack" are different findings and a
counter that cannot separate them is not evidence.

---

## 3. Item 2 — the deterrence dividend (`deterrence_gain`)

Human, 2026-08-06: *"we aren't valuing defensive units highly
enough... a musket behind walls is pretty dang strong."*

Priced **marginally**, because one more defender is a marginal
question: run the defence read twice, once with a virtual unit of the
candidate type standing in the city and once without, and take the
difference in expected attacker shields destroyed. The tile-aware
`get_total_defense_power()` seam (`deepcombat.c:174`) is inside that
read, which is what makes "behind walls" arithmetic rather than
rhetoric — the same musket prices higher in a walled city because its
duels really are better.

Times `deep_garrison_menace()`, the same 0..1 pressure read the
garrison floor is already sized by, so the term that insures against a
threat and the term that prices killing it agree about how much threat
there is.

**Not a reimplementation.** CONSEQ's fight-vs-deny design named this
exact quantity from the other side — "what resisting COSTS the taker"
(`findings/conseq-wrace-2026-08-07.md` §2b). Same term, consumed.

**The double-count guard the brief demanded.** `defence_worth_mult`
prices the CITY, at the counter-claim's stakes read. This prices the
ATTRITION. They are disjoint quantities applied by different seams and
they are added, never multiplied.

Applied at both defender-want sites (first defender and ordinary
defender). Returns 0 before running any DP when the city faces no
wave, so peacetime costs nothing.

---

## 4. Item 3 — walls as pop-loss insurance (`pop_insurance_gain`)

Stock: `EFT_UNIT_NO_LOSE_POP` at `units × 2` (`daieffects.c:456`) — a
headcount WAG that never asks whether the city is under attack, so a
walled interior capital and a walled border city with an army outside
it are worth the same.

The mechanic is exact and CONSEQ verified both halves:

- **Capture costs one size point, flat and once**
  (`server/citytools.c:2141`). Walls do **not** insure that.
- **Every successful attack costs another** under `killcitizen` +
  `UCF_KILLCITIZEN` absent this effect (`server/unithand.c:4601`).
  That is what the effect removes, and it is what is priced.

So: `E[successful attacks] × the citizen's stream value`, where the
first factor is the DP's new `hits` and not a guess, capped at the
citizens the city has left to lose (the server cannot reduce below
size 1; the step past that is the capture). The citizen is valued at
`deep_unworked_tile_value()` — the same valuation the health credit
beside it uses, so the two are directly comparable and this gain
carries the same horizon scale `horizon_health_weight` does.

**No double count of Defend_Bonus**: that bonus is already inside the
odds, because every duel is priced through the tile-aware defence
seam. This is the additional stream on top. A city already carrying
the effect from elsewhere (civ2civ3 gives it to the Great Wall,
`effects.ruleset:3123`) is owed nothing.

Deliberately **not** gated on `!threatened` — gating insurance on
peace would gate it on having no population — so the wartime skip
gains an exemption beside culture's.

---

## 5. Item 4 — the barracks as two streams (`heal_stream_gain`,
`veteran_stream_gain`)

Stock pays `5c + num` for `EFT_HP_REGEN` (`daieffects.c:459`) and
`amount × (3c + num)` for `EFT_VETERAN_BUILD` (`:479`). Two headcount
WAGs for two things that are streams. Amortization audit 1c.

**HP throughput.** The ruleset states the rate exactly —
`hp_gain_coord()` (`common/unit.c:2216`) heals `base_hp × regen%` per
turn — so what the building buys is *turns of the line*, and the
population it buys them for is the damaged units standing in this
city. Per unit: turns to full at today's regen minus turns to full
with the building's, each saved turn worth that unit's shields at
`SHIELD_WEIGHTING`. Capped at the horizon rather than called infinite
when regen is zero. **A city with no casualties is owed nothing**,
which is the point — stock pays for a Barracks in a city that has
never seen a fight.

**The veteran margin**, the stream stock most plainly prices as a
point: a power multiplier for the unit's whole life. Both factors are
the ruleset's own numbers. The margin is `power_fact` at the level the
city would build at over `power_fact` at today's
(`utype_veteran_level()`), minus one; a unit that fraction stronger is
worth that fraction more shields — the identity the attack and defence
ratings already use. The count is `deep_horizon_turns()` over the
turns to build one. Priced on the best defender the city can build, so
an attacker-heavy city is under-credited: the safe direction.

Counted apart in one `deep-barracks` line, because a session that
cannot tell which stream moved has learnt nothing.

### 5a. And it is EMPTY in civ2civ3 — with a diagnosis that is bigger
than the item

`deep-barracks` is **0.0 a game**. Chased rather than blamed on the
probe length, because the answer is not about turns.

civ2civ3's Barracks *has* both effects and is buildable from turn 1
(no reqs at all, `buildings.ruleset:262`), so both credit functions
are reached — 363 times for Barracks in a single game. But the
effects are gated on a **building FLAG**, not a building:

    [effect_barracks_veteran]  Veteran_Build 1
      reqs = { "BuildingFlag", "Barracks", "City", TRUE
               "UnitClassFlag", "Barracks", "Local", TRUE ... }
    [effect_barracks_regen]    HP_Regen 100
      reqs = { "BuildingFlag", "Barracks", "City", TRUE ... }

(`effects.ruleset:1824`, `:1835`; three buildings carry the flag.)

`get_potential_improvement_bonus()` finds effects via
`get_req_source_effects()` keyed on a **`VUT_IMPROVEMENT`** source. An
effect whose requirement is **`VUT_IMPR_FLAG`** is indexed under a
different source and is never returned. Measured directly, 363 times
in one game:

    DBGV impr="Barracks" vb_poss=0 vb_cert=0 hp_poss=0
         have_vb=0 have_hp=0

`RPT_POSSIBLE` in place of `RPT_CERTAIN` does not help — tried, still
zero. The problem is the index, not the problem type.

> **`get_potential_improvement_bonus()` is blind to every effect a
> ruleset gates on a building FLAG rather than a building.** Deep uses
> that accessor in the growth credit, the health credit, the content
> credit, and these two. Any flag-gated effect in any ruleset is
> priced at zero by all of them. This is very likely *why* stock
> prices `HP_Regen` and `Veteran_Build` with headcount WAGs instead of
> reading them — and `daieffects.c:479` carries the matching FIXME,
> "check other reqs (e.g., unitflag)".

**Not fixed here, deliberately.** Doing it right needs the effect
evaluated against a context that knows the candidate building's flags
— a virtual city, or the `struct req_context` field `00-context.md` §3
already lists as future work. That is a slice, not a patch at the end
of a session, and rushing shared requirement-evaluation code is how
this project breaks its own control.

**Item 4 therefore ships built, off, and zero-population.** Its
arithmetic is unexercised, so its two knobs are untuned *and* untested
against live data. Do not tune them until the accessor can see the
effect.

> **Correction (2026-08-12, session CREDIT).** The defect is real and
> the measurement above is right; the *mechanism* named in this section
> is wrong on both counts, and the correction matters because it is
> what made the fix small.
>
> The effect **is** returned. `effect_req_append()`
> (`common/effects.c:276`, in tree since 2022) indexes an effect with a
> `VUT_IMPR_FLAG` requirement under **every improvement carrying that
> flag**, so `get_req_source_effects()` on the Barracks hands it back.
> What drops it is the requirement loop: the accessor short-circuits a
> requirement naming the improvement *itself* (the city does not have
> the building yet) and evaluates everything else live, so
> `"BuildingFlag", "Barracks", "City"` is asked of a city with no
> barracks and is **definitively FALSE**. That is also why
> `RPT_POSSIBLE` did not help — the requirement is false, not unknown.
>
> And the building flag was only half of it. Both effects also carry
> `"UnitClassFlag", "Barracks", "Local"`, which with no unit in the
> context *is* unknown, and `RPT_CERTAIN` discards it. Fixing only the
> building half left `deep-barracks` at 0.00 in a smoke game — measured,
> which is the only reason the second half was found.
>
> Both halves are fixed on `epic/credit-integrity`
> (`ai/deep/deepeffects.c`): the candidate satisfies a flag requirement
> the way it satisfies a requirement naming itself, and the two barracks
> credits ask on behalf of the unit they are about. `deep-barracks` goes
> 0 -> 109 lines in one smoke game. The knobs stay closed; see that
> session's STATUS entry for why the first census makes the veteran term
> look far too large.

---

## 6. Item 5 — unit lifetime upkeep at the build seam
(`unit_upkeep_charge`)

Amortization audit 1b, and the item the brief singled out for its own
A/B because it moves every unit want.

A building's gold upkeep is charged against its horizon credit over
MORT (`deephorizon.c`, `upkeep_gold_weight`). A unit's upkeep over its
lifetime is charged **nowhere** at build time. Only the defence FEAR
terms carry a partial discount (`upkeep_unit_weight`), which is a
different seam pricing a different question and never reaches the
settler, the worker, the ferry or the attacker. So a Warriors kept 200
turns costs about twenty times its build price and the auction is told
it costs the build price.

Charged at `deep_city_choose_build()`'s candidate seam — where every
class exists and nothing has consumed the bid, the same place the
endgame deflator and the switch debit already charge — so it moves
military and domestic wants alike.

`E[lifetime]` is `deep_horizon_turns()`, the horizon the building side
uses, truncated by CONSEQ's `deep_utype_successor_eta()` where a
successor is on the way: that is the turn deep's own rotation melts
the unit.

**No scale is minted.** Shield upkeep converts at `SHIELD_WEIGHTING`
(the codebase's own shield-to-want constant) and gold upkeep at
`upkeep_gold_weight` (the building side's knob, same dimension —
"gold per turn against a horizon").

**Food upkeep is deliberately NOT charged.** This seam has no
food-to-want conversion and inventing one is rule 7. The charge
therefore under-counts, which is the safe direction for a mechanism
whose whole effect is to subtract.

**The exemption that caps its reach, stated rather than buried.**
Emergency-tier choices are exempt, as every deep gate exempts them. The
derby-3 birth guards — this item's own motivating population — fire at
`DWT_EMERGENCY` under the *inherited* rule, so they are out of reach
until `birth_guard_gain` is open, at which point CONSEQ re-prices them
to `DWT_NORMAL` and they come in. The two knobs interact and should be
swept together.

### 6a. The census that forced a seventh commit

The item shipped gold-and-shield-only, on the reasoning that this seam
has no food-to-want conversion and that omitting food was "the safe
direction". **The counter said otherwise: zero `deep-upcharge` lines
in the ON arm.** Instrumenting one 40-turn contested game and logging
every candidate:

    616  evaluations reached the charge
    321  got as far as reading an upkeep
      5  were actually charged

    pick / upkeep as read                          n
    Settlers  ups=0 upg=0 upf=1  Despotism        184
    Settlers  ups=0 upg=0 upf=1  Tribal            75
    Settlers  ups=0 upg=0 upf=1  Anarchy           22
    Diplomat  ups=0 upg=0 upf=0  Despotism         14
    Workers   ups=0 upg=0 upf=0  Despotism          8
    Phalanx   ups=0 upg=0 upf=1  Despotism          7
    Warriors  ups=0 upg=0 upf=1  Despotism          4
    Phalanx   ups=1 upg=0 upf=1  Tribal             2

**Under Despotism, Tribal and Anarchy — the governments deep is in for
most of a contested game — civ2civ3 charges almost every unit no gold
and no shields. The only nonzero upkeep in the venue is FOOD.** So
omitting food was not under-charging in a safe direction; it charged
nothing. And the audit's own headline population is a **Settler**,
whose entire lifetime cost in this ruleset *is* the food that
suppresses the city it came from.

Fixed by summing gold + food under `upkeep_gold_weight` — the
equivalence `deep_defender_upkeep_discount()` already makes forty
lines up in the same file, so it is rule 3 reuse and not rule 7
invention. After: **299 charges a game, 281 of them Settlers.**

### 6b. The second finding, which is not this seat's to fix

The charge is now correctly dimensioned and still negligible against
what it is subtracted from:

    deep-upcharge pick="Settlers" life=24 upf=1 debit=3.0
                  want=750.1 left=747.1

**3 want against 750 is 0.4%.** `upkeep_gold_weight` is 0.125, so even
at the top of `unit_upkeep_charge`'s registered range (4.0) the charge
is 1.6% of a settler bid. The mechanism cannot move this auction, and
the reason is not the mechanism: a charge calibrated on the BUILDING
side — where a 1-gold building costs 3 want against credits of tens —
is invisible on the UNIT side, where a scaled domestic settler bid is
750.

That is **RC4, incommensurable want scales, measured with a ratio
attached**, and it is W01's problem rather than this seat's. I did not
widen the knob's range to paper over it, because choosing that width
would be exactly the invented constant rule 7 forbids.

---
## 7. Items 6 and 7 — parked, with evidence

The brief made these stretch and said to park them with evidence
rather than rush them. Both are parked, and the evidence is stronger
than "ran out of time".

### 7a. Item 6, the Defend_Bonus sweep (audit 2c)

**The defect is confirmed and its seam is named.** civ2civ3 carries 13
`Defend_Bonus` effects across four distinct carriers answering
distinct threat classes — City Walls (Ground), Coastal Defense (sea),
SAM Battery (air), SDI Defense.

Both stock and deep collapse that whole family to **one** building:

    wall_id = dai_find_source_building(pcity, EFT_DEFEND_BONUS, NULL);

under stock's own comment *"HACK: This needs changing if multiple
improvements provide this effect"* (`daimilitary.c:1896`, and again at
`:962`). Deep inherited the hack at `deepdefence.c:4210`, where
`wall_fear_weight` routes **all** injected fear — boat shadow, air,
barbarian, density — into whichever single building that call happens
to return. So the coastal fear a city carries can be buying City
Walls, which do nothing about ships.

**Why the era-gated half stays parked.** M4a's era-reachability table
(now closed; `benchmark/era-check.py` and findings on `epic/m4a`)
gives the tech frontier at cell end:

    smoke, primary-120        -> Gunpowder
    contested, isolated-150   -> Sanitation at best
    lategame-80               -> Espionage once in 20
    garden2b (2p, barbs off, T301 development race) -> Flight 11/24

**No combat venue in the portfolio reaches Flight.** SAM and SDI are
behind it, so pricing them would be pricing units the portfolio never
fields, and their credit could never be measured. Coastal Defense
*is* in scope (pre-Flight) and is the member a sweep should carry
first.

### 7b. Item 7, incite resistance (audit 2d)

Parked with a pointer, which is what the brief asked for.

M4a **has** built the attacker side: `epic/m4a` commit `633bcca985`
adds `ai/deep/deepspy.c` (449 lines) and prices incite among four
families, replacing stock's self-described arbitrary ladder
(`daiactions.c:96` — incite 7000).

Not consumed here, for two reasons and neither is a judgement call:

1. It had published no numbers while this session was open, and its
   branch was still in flight.
2. Consuming it means merging an in-flight epic branch into this one,
   which is bundling two workstreams — refused by `CLAUDE.md` and by
   this brief's own instruction not to touch that branch.

M4a's era table also reports **Espionage at 7/24 on garden2b and 1/20
on lategame-80**, so Spies barely exist in the portfolio and the
defender-side credit would be small even once built.

The pointer: `epic/m4a:ai/deep/deepspy.c` carries the
P(hostile incite attempt) half. Whoever picks up
`EFT_INCITE_COST_PCT` (WAG-priced at `daieffects.c:218`) after that
branch merges must reuse it and must not build a second spy model.

---

## 8. The A/B: both arms null on score, every mechanism live

Three arms, one binary, one shared OFF arm serving both comparisons —
so the item-5 table and the bundle table are read against the *same*
20 games and are directly comparable to each other.

    venue      contested, 20 paired seeds per arm, --arms deep
    binary     the final pin (9275e9bb6d), key cff0da39e9dd3d, all arms
    OFF        benchmark/tuning/identity.ini   (every knob at default)
    item 5     benchmark/tuning/m4b-upcharge.ini  unit_upkeep_charge 1.0
    bundle     benchmark/tuning/m4b-bundle.ini    all six gains at 1.0
    pairing violations 0 in every arm

**Provenance.** The runs landed in the session scratchpad under `/tmp`,
which has since been reclaimed, so — as with CONSEQ/WRACE — every table
below is the analysis output as it was produced, not a re-derivable
artefact. The commands, the tuning files and the pin sha are recorded
here and in `sessions/progress-M4b.log` so the cells can be re-run.

### 8a. Item 5 alone (`unit_upkeep_charge = 1.0`)

    paired seeds: 20  (off 20, on 20)
    metric                       OFF        ON     delta      se      t   wins
    score                      85.10     83.29     -1.81    2.79  -0.65   6/20
    cities                      4.59      4.37     -0.22    0.18  -1.24   7/20
    unitsbuilt                 46.46     45.17     -1.29    1.57  -0.82   9/20
    techs                      19.17     19.24     +0.07    0.67  +0.11  11/20
    pop                        26.82     24.91     -1.91    0.76  -2.51   6/20
    mean_city_size              5.94      5.95     +0.01    0.21  +0.04   6/20
    settlers                    0.13      0.12     -0.01    0.03  -0.18   7/20
    landarea                50387.50  48531.25  -1856.25 1533.89  -1.21  10/20
    munits                     13.39     12.61     -0.78    0.75  -1.04   7/20
    wonders                     0.36      0.31     -0.04    0.05  -0.89   6/20
    unitslost                  13.24     13.20     -0.04    1.47  -0.03   9/20
    unitskilled                 8.47      8.51     +0.04    1.28  +0.03   9/20

    upcharge                    0.00   1551.00  +1551.00  103.11 +15.04  20/20
    upchargesum                 0.00    715.75   +715.75   28.98 +24.70  20/20

**Score is null** — `-1.81 ± 2.79`, `t = -0.65`, 6/20 wins — which is
the null §6b predicted from the census: a 3-want debit against a
750-want settler bid cannot move an auction, and the range cannot
reach far enough to make it.

**The mechanism is unambiguously live**: 1551 charge lines a game
against zero in the off arm, 20/20 seeds, `t = +15.04`.

**The one flag in the table is `pop`** — `-1.91`, `t = -2.51`, the
only |t| above 2 in either A/B. It is one metric out of twelve at 20
seeds, so it is a flag and not a finding; but it points the way the
mechanism points, because 281 of the ~299 charges a game land on
Settlers (§6a) and a settler tax is a population tax. Two readings
survive the data — noise in a multiple-comparison table, or a charge
that is negligible on the average bid and still flips the marginal
settler — and this cell cannot separate them. **Do not open this knob
before the scale question in §6b is settled**, and when it is opened,
watch `pop` first.

### 8b. The bundle (all six gains at 1.0)

    paired seeds: 20  (off 20, on 20)
    metric                       OFF        ON     delta      se      t   wins
    score                      85.10     83.39     -1.71    2.49  -0.68   6/20
    cities                      4.59      4.47     -0.12    0.18  -0.67  10/20
    unitsbuilt                 46.46     43.41     -3.06    1.60  -1.91   6/20
    techs                      19.17     19.01     -0.16    0.59  -0.27   8/20
    pop                        26.82     25.71     -1.12    0.85  -1.32   9/20
    mean_city_size              5.94      5.86     -0.08    0.23  -0.36   9/20
    settlers                    0.13      0.18     +0.05    0.04  +1.25   8/20
    landarea                50387.50  49300.00  -1087.50 1484.52  -0.73  11/20
    munits                     13.39     12.88     -0.51    0.63  -0.80   5/20
    wonders                     0.36      0.37     +0.01    0.06  +0.22   7/20
    unitslost                  13.24     11.24     -2.00    1.44  -1.39   6/20
    unitskilled                 8.47      8.07     -0.40    1.21  -0.33   7/20

    sortie                     36.70     39.10     +2.40    3.99  +0.60  12/20
    sortieno                    0.00      8.95     +8.95    2.04  +4.40  17/20
    deter                       0.00    273.50   +273.50   42.65  +6.41  18/20
    detersum                    0.00    192.30   +192.30   29.96  +6.42  18/20
    popins                      0.00     93.80    +93.80   16.31  +5.75  17/20
    barracks                    0.00      0.00     +0.00    0.00   +nan   0/20
    upcharge                    0.00   1583.35  +1583.35  102.07 +15.51  20/20

**Score is null again** — `-1.71 ± 2.49`, `t = -0.68`, 6/20 wins. At
face value 1.0, six untuned gains together do not move the contested
cell. That is the expected result for knobs whose levels have never
been swept, and it is the reason they ship off.

The interesting rows are the ones that are not score. The bundle
**builds 3.06 fewer units** (`t = -1.91`) and **loses 2.00 fewer**
(`t = -1.39`) — the shape a deterrence bundle should have, a smaller
and better-priced army, at exactly the significance a 20-seed cell can
be expected to give it: none. Neither row is a claim; both are the
first thing a tuning pass should re-take at power.

`barracks 0.00` in the ON arm is §5a stated as an A/B row: the two
barracks knobs have **no population at all** in this venue.

### 8c. The per-line census behind those counters

Same 20 bundle games, every gain at 1.0, read out of the game logs —
this is what the mechanisms actually computed, as opposed to how often
they fired:

    deter marginal shields   n=5461  mean= 15.81  median=  7.40  max= 143.9
    deter menace             n=5461  mean=  0.99  median=  1.00  max=   1.0
    deter dividend paid      n=5461  mean= 15.74  median=  6.90  max= 143.9
    popins credit per line   n=1876  mean= 44.11  median=  7.85  max= 454.7
    sortie prize TAKEN       n=  36  mean= 29.72  median= 30.00  max=  60.0
    sortie score TAKEN       n=  36  mean= 28.70  median= 30.00  max=  60.0
    sortie prize DECLINED    n= 179  mean= 33.85  median= 30.00  max= 150.0

Three things are settled by that block.

1. **The deterrence dividend is a real quantity, not a rounding
   error**: a median defender imposes 7.4 expected attacker shields
   and the tail reaches 143.9. Menace sits at 1.000 almost everywhere
   in this venue, so on contested the dividend is essentially
   unattenuated and the knob is the whole lever.
2. **Pop insurance is the right order of magnitude against the
   building credits it sits beside** (median 7.85, mean 44.11 over
   1876 lines), which is what §4 needed to be true for the credit to
   be tunable at all.
3. **The declined counter earned its place.** Across the 20 games the
   pass declined 179 prizes (8.95 a game) and took 36 kills (1.8 a
   game) — a 5:1 decline rate — and the declined prizes are
   *larger* on average (33.85 vs 29.72, max 150 vs 60). So "the pass
   never sees a big stack" is ruled out by measurement: it sees them
   and its bar turns them down. Whether the bar is right is a tuning
   question, and it is now a question with data under it rather than a
   guess.

### 8d. What these two nulls do and do not license

They license exactly one sentence: **at face value 1.0, on contested,
20 seeds, none of these mechanisms moves score, and all of them except
the barracks pair execute.** They do not say the mechanisms are worth
nothing — no level has been swept, contested is one venue, and the
largest live term (`deterrence_gain`) has never been run against the
two knobs that fund the same body from other sides
(`defence_worth_mult`, `survival_fear_value`). A face-value probe is a
population check, not a verdict.

---

## 9. Off-switch proof

    smoke, 10 paired seeds, identity.ini on BOTH binaries
    base  6fdcd6cfd9   vs   final  9275e9bb6d

    10/10 scorelogs BYTE-IDENTICAL.

The same proof also passed **10/10** at the intermediate head
`03620395b4` (all six commits before the food fix), so the identity is
established twice across the session rather than once at the end.

Every mechanism is additionally inert by construction at its default:
the sortie keeps best-chance selection and computes no prize; the
dividend returns before running any DP; the upkeep charge returns
before reading an upkeep; the pop-insurance and barracks credits
return before their first lookup, and the per-city DP the insurance
needs is never run. Their counters are correspondingly absent from the
off arm.

`meson test -C build` **4/4 OK**. `python3 benchmark/tune-audit.py`
**CLEAN 202/202/202/202** (was 196 at session start).

---

## 10. Six knobs, all PLACEHOLDER, all off

    sortie_prize_gain     0.0   killstack prize in the sortie score
    deterrence_gain       0.0   E[attacker shields] x menace
    pop_insurance_gain    0.0   E[successful attacks] x citizen value
    heal_stream_gain      0.0   HP throughput  (NO POPULATION, §5a)
    veteran_stream_gain   0.0   veteran margin (NO POPULATION, §5a)
    unit_upkeep_charge    0.0   lifetime upkeep at the build seam

First runs owed, in the order the A/B census points (§8b, §8c):

1. **`deterrence_gain`.** The largest live term and the one with real
   marginal arithmetic behind it: 5461 priced lines over 20 games,
   median 7.40 and mean 15.81 expected attacker shields, menace 1.000
   almost everywhere on contested. Sweep it against
   `defence_worth_mult` and `survival_fear_value`, which fund the same
   body from other sides.
2. **`pop_insurance_gain`.** Live at 1876 lines over 20 games, median
   credit 7.85 / mean 44.11 — the right order against the building
   credits it sits beside.
3. **`sortie_prize_gain`.** Live but rare, and its bar is already
   *selective* rather than starved: 179 declines against 36 kills over
   20 games, with the declined prizes the larger ones (§8c). The bar,
   not the population, is what a sweep would be moving.
4. **`unit_upkeep_charge`.** Do not tune before reading §6b and §8a: at
   face value it is 0.4% of a settler bid, the registered range cannot
   reach, and the one metric it did move at 1.0 was `pop` (`-1.91`,
   `t = -2.51`) in the wrong direction.
5. **`heal_stream_gain` / `veteran_stream_gain`.** Do not tune at all
   until the accessor in §5a can see a flag-gated effect. `barracks`
   is a literal `0.00` A/B row (§8b).

**None of the six may be opened on this evidence.** Both arms are null
on score at face value; what the session established is that five of
the six mechanisms have population and correct dimensions, which is
the precondition for tuning and not a substitute for it.
