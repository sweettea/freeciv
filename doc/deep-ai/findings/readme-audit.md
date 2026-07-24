# Part 12 — Auditing `doc/README.AI`'s own list

Companion to Parts 1–11 and the PRD. Same tree: `freeciv/freeciv` @ `0896b34`.

I'd only picked off the README items that happened to intersect other
investigations. Going through it properly: **five live bugs we hadn't covered**,
two stale entries, and a third independent in-source confirmation of RC4.

---

## 1. Status board

### THINGS THAT NEED TO BE FIXED

| # | Item | Status |
|---|---|---|
| 1 | Cities don't realize units are on their way to defend it | **NOT COVERED** — live, §2.1 |
| 2 | AI builds cities without regard to danger at that location | **Covered** — Part 6 (routing) + Part 4 §1 |
| 3 | AI won't build cross-country roads outside city radii | **Covered** — PRD §4.4 |
| 4 | Locally_zero_minimap not implemented when wilderness changes | **STALE** — identifier no longer exists, §2.6 |
| 5 | If no path to victim found, new victim should be chosen | **Partly** — §2.4 |
| 6 | Emergencies in two cities at once aren't handled properly | **NOT COVERED** — live, §2.2 |
| 7 | Explorers won't use ferries; AI won't build units to explore | **Covered** — Part 10 §1 |
| 8 | AI thinks wasting a horde of weak units to kill one enemy is profitable | **Partly** — §2.5, deeper than it looks |
| 9 | Stop building shore defense in landlocked cities with a pond adjacent | **NOT COVERED** — live, and **the fix data already exists unused**, §2.3 |
| 10 | Fix AI valuation of supermarket (never builds it) | **Covered** — PRD §4.2 fixes the class |
| 11 | Teach the AI to coordinate units in an attack | **Covered** — PRD §4.5 |

### IDEA SPACE

| Item | Status |
|---|---|
| Friendly cities can be used as beachheads | **Partly** — §2.7 |
| Assess_danger should acknowledge positive feedback between multiple attackers | **NOT COVERED** — live, §2.8 |
| Bodyguard and charge should meet en-route more elegantly | **NOT COVERED** — minor, §2.9 |
| `struct choice` should have a priority indicator, removing capped wants | **Covered — and it's PRD §3.2 / RC4**, §3 |

### Narrative claims

| Claim | Status |
|---|---|
| *"The version of amortize used in the military code (military_amortize) remains a complete mystery."* | **STALE** — §2.6 |
| *"kill_desire... isn't perfect: multi-victim part is flawed"* | **Live and diagnosable** — §2.5 |
| *"Amortization_Factor is completely beyond me"* | Partly stale |
| Ferry system as statistical mechanics | **Covered** — PRD §4.11 |
| Alliance: *"will happily hand over any tech and maps"* | **Misleading** — Part 5 §6c; it's team-only proactively |

---

## 2. The live items we hadn't covered

### 2.1 — Cities don't know defenders are coming

**Confirmed, and there's a telling asymmetry.**

`assess_defense_backend()` (`daimilitary.c:329`):

```c
unit_list_iterate(pcity->tile->units, punit) {
  defense += assess_defense_unit(ait, pcity, punit, igwall);
} unit_list_iterate_end;
```

Only units **physically standing on the city tile**. A defender two turns out
with `task == AIUNIT_DEFEND_HOME` and `goto_tile == pcity->tile` contributes
nothing. So while three defenders are walking toward a threatened city, that city
keeps computing `danger > defense` and keeps requisitioning more — at
`DAI_WANT_MILITARY_EMERGENCY` priority, which outranks everything domestic.

**The asymmetry:** the AI already does exactly this bookkeeping for *enemy*
cities. `reinforcements_cost_and_value()` fills
`city_data->invasion.attack/occupy` (`daicity.h:31`) so units can see how many
friends are converging on a target. The same accounting for one's own cities
simply doesn't exist.

**Fix.** Mirror the structure — an `incoming_defence` counter per own city,
populated each phase by iterating units whose task is `AIUNIT_DEFEND_HOME` and
whose `goto_tile` is that city, weighted by ETA and survival probability. Subtract
from effective danger. ~40 lines, reusing an existing pattern.

**Why it matters here:** without it, PRD §4.5's campaign object will fight the
defence system. A campaign draws units away; the drained cities panic and
requisition replacements at emergency priority; production oscillates. Fix this
*before* campaigns, not after.

### 2.2 — Two simultaneous emergencies

**Confirmed.** `dai_manage_cities()` (`daicity.c:875`):

```c
city_list_iterate(pplayer->cities, pcity) {
  if (CITY_EMERGENCY(pcity)) {
    auto_arrange_workers(pcity);
  }
  if (CITY_EMERGENCY(pcity)) {
    resolve_city_emergency(ait, pplayer, pcity);
  }
  dai_city_sell_noncritical(pcity, TRUE);
  sync_cities();
} city_list_iterate_end;
```

Each emergency is resolved in isolation, in **city list order**, with no
arbitration. `resolve_city_emergency()` rearranges the city's own tiles and
disbands/rehomes its supported units — so the first city in list order grabs what
it needs and the second gets the remainder, regardless of which matters more.

Worse, both compete for the same gold via `dai_spend_gold()` and
`maxbuycost`, again first-come.

**Fix.** Two passes. Collect all emergencies with severity (`urgency`,
`grave_danger`, city `worth`, whether it's a capital / wonder city / migration
magnet), sort, then resolve in priority order against a shared budget. The
severity inputs all exist; only the sort and the budget are missing.

Note this composes with §2.1: once cities know help is coming, fewer of them are
in "emergency" at once, and the arbitration has less to do.

### 2.3 — Shore defence in a city next to a pond

**Confirmed, and the fix is already computed and sitting unused.**

`daieffects.c:543–547`:

```c
|| (adv->threats.invasions
    /* FIXME: This ignores riverboats on some rulesets.
              We should analyze rulesets when game starts
              and have relevant checks here. */
    && is_terrain_class_near_tile(&(wld.map), pcity->tile, TC_OCEAN)))
```

`is_terrain_class_near_tile(TC_OCEAN)` is true for **any** adjacent oceanic tile,
including a one-tile lake. So a landlocked city next to a pond wants coastal
defence against an invasion that cannot physically arrive.

**But** `advdata.c:301–315` already builds this:

```c
adv->oceans = fc_calloc(adv->num_oceans + 1, sizeof(struct adv_area_info));
...
} else {
  adv->oceans[-cont].size++;
}
```

`struct adv_area_info { int size; bool threat; }` — and grepping usage,
**only `.threat` is ever read** (`daieffects.c:524, 529`). The `size` field is
computed every phase for every ocean and **never consulted anywhere**.

**Fix, roughly five lines:** gate the shore-defence want on the adjacent water
body's `size` exceeding a threshold (say, large enough to hold a transport route),
via `adv->oceans[-tile_continent(adjacent_tile)].size`. A twenty-year-old README
bug whose required data has been sitting there the whole time.

The in-place FIXME about riverboats also folds neatly into PRD §6.1's ruleset
probe: analyse at game start which unit classes can reach which water bodies.

### 2.4 — No path to victim → choose a new victim

**Partly handled.** In `find_something_to_kill()`, unreachable targets are
`continue`d during *selection*, so the initial choice is sound.

The gap is **mid-journey**: once a unit is committed and the path later fails, the
handling is thin. The bodyguard path (`daiunit.c:751`) does
`dai_unit_new_task(ait, punit, AIUNIT_NONE, nullptr)` — dropping the task
entirely rather than retargeting, so the unit idles until the next full
re-evaluation.

**Under PRD §4.5 this largely dissolves**: a campaign has a target, a deadline,
and abort conditions, so a blocked path triggers re-plan-or-abort at the campaign
level rather than per-unit amnesia. Worth adding explicitly to the campaign abort
conditions: *"no viable path for ≥N turns."*

### 2.5 — The horde of weak units, and `kill_desire`

The README flags these separately but they're the same bug.
`kill_desire()` (`daiunit.c:341`):

```c
desire = (benefit * attack - loss * vuln) * victim_count
  * SHIELD_WEIGHTING / (attack + vuln * victim_count);
```

Two problems.

**(a) `benefit` is one victim's value, multiplied by `victim_count`.** That's only
correct under `killstack` (a real server setting, `settings.c:2455`). With
`killstack` off, killing the top defender of a stack of six yields *one* unit's
worth of benefit, not six. The AI has no `game.info.killstack` check anywhere,
so it over-values attacking stacks in rulesets where stacks don't die together.

**(b) The win model is linear where combat is not.** `attack / (attack + vuln)`
is a plausible single-combat estimate, but the multi-victim form
`(A + V·n)` in the denominator with `n` in the numerator produces a value that
*saturates* rather than *collapsing* as `n` grows. Sequential combats compound
multiplicatively — roughly `(A/(A+V))^n` for taking a whole stack — so a horde of
weak attackers against several defenders should be valued far lower than this
formula says. That is exactly the README's *"wasting a horde of weak military
units to kill one enemy is profitable (PR#1340)."*

**Fix.** Gate the `victim_count` multiplication on `game.info.killstack`, and
replace the linear multi-victim term with a compounding survival model.

This is a **prerequisite for PRD §4.5's `required_attack`** — that calculation
inherits `kill_desire`'s assumptions, so a campaign sized with a broken combat
model will muster the wrong force. Add it to the campaign workstream.

Note also the defence/danger asymmetry this creates: defence aggregates via
`assess_defense_quadratic()` (superlinear), while threat aggregates via a plain
`total_danger += vulnerability` sum (`daimilitary.c:939`). See §2.8.

### 2.6 — Two stale entries

**`locally_zero_minimap`** — `grep -rn "locally_zero_minimap\|minimap" ai/
server/advisors/` returns nothing. The identifier no longer exists; the TODO
outlived the code it referenced. **Delete from README.**

**`military_amortize` "remains a complete mystery"** — it isn't, any more.
`daitools.c:117`:

```c
/* Amortize a want modified by the shields (build_cost) we risk losing.
 * We add the build time of the unit(s) we risk to amortize delay. ... */
adv_want military_amortize(struct player *pplayer, struct city *pcity,
                           adv_want value, int delay, int build_cost)
{
  int output = MAX(pcity ? pcity->surplus[O_SHIELD] : 1,
                   ai->stats.average_production);
  int build_time = build_cost / MAX(output, 1);
  return amortize(value, delay + build_time);
}
```

Documented, comprehensible, and reasonable: discount by travel delay *plus* the
time to replace what you might lose. Someone fixed this and never updated the
README. **Worth a doc patch** — it's a small kindness to the next person who
reads that file expecting a minefield.

### 2.7 — Friendly cities as beachheads

`find_beachhead()` exists (`daiunit.c:1135`) and picks a landing tile near the
target. The README's idea is different: **land in a friendly or allied port
instead**, which skips the contested beach landing entirely.

Under PRD §4.5 + Part 5 §2, this is the amphibious form of allied staging: a
`muster_tile` that happens to be an overseas allied city. Add allied and own
overseas cities as candidate `ferry_dest` values in `find_beachhead`, ranked
above raw beaches. Small change, meaningful behaviour.

### 2.8 — Positive feedback in `assess_danger`

**Live, and it's an asymmetry.** `daimilitary.c:939`:

```c
total_danger += vulnerability;
```

A plain sum. Meanwhile the defence side uses `assess_defense_quadratic()` — a
superlinear aggregation, on the correct principle that two defenders in a city
are worth more than twice one.

The same principle applies to attackers and isn't modelled: two attackers can
overwhelm a defender neither could beat alone, because the first softens and the
second kills. So the AI systematically **under-estimates the danger of
concentrated enemy stacks** while correctly valuing its own concentration.

**Fix.** Aggregate danger with the same superlinear function used for defence.
This has a satisfying consequence: it's the mirror of PRD §4.5. Once the AI
values *its own* concentration correctly (campaigns) it should also fear *the
enemy's*, and both fall out of one shared aggregation function. Write it once,
use it on both sides.

### 2.9 — Bodyguard and charge meeting en route

Minor, but real: the rendezvous is the same forget-and-re-plan pattern as the
ferry system. Under PRD §4.5 both units are campaign members with a shared
`muster_tile`, so the rendezvous problem dissolves rather than needing its own
solution. Note it as "resolved by campaigns," not a separate workstream.

---

## 3. A third confirmation of RC4

The PRD argues (RC4) that incommensurable want scales are the deepest structural
defect, citing README's IDEA SPACE and `set_default_city_manager`'s comment.
There's a third, in `daicity.c:130`:

```c
/* This function must convert from units of 'building want' to 'tech want'.
 * We put this conversion in a function because the 'want' scales are
 * unclear and kludged. Consequently, this conversion might require tweaking. */
static void want_tech_for_improvement_effect(...)
```

Three independent authors, in three subsystems, over presumably many years, each
documenting the same problem and each working around it locally:

1. `README.AI` IDEA SPACE — *"reduce the number of 'special' want values and
   remove the necessity to have want capped, thus reducing confusion"*
2. `set_default_city_manager()` — *"those values are on a different scale"*, so
   AI priorities were **severed** from citizen arrangement
3. `want_tech_for_improvement_effect()` — *"the 'want' scales are unclear and
   kludged"*

**This is the strongest available argument for making the want-scale unification
P0.1.** It isn't tidiness; it's the defect that three separate subsystems have
each independently routed around, and each workaround costs real play strength.

---

## 4. Also in README, worth noting

**The stated long-term goals** are worth quoting in any upstream proposal:

> *-> to create a challenging and fun AI for human players to defeat*
> *-> to create an AI that can handle all the rules possibilities that Freeciv
> can offer*

The first justifies PRD Part V and Part 11 §6 (difficulty as competence, honest
information) as **on-mission**, not scope creep. The second justifies PRD §6.1's
ruleset probe. Lead with these.

**The diplomacy warning** is worth taking seriously:

> *The introduction of diplomacy is fraught with many problems. One is that it
> usually gains only human players, not AI players, since humans are so much
> smarter and know how to exploit diplomacy, while for AIs they mostly only add
> constraints on what it can do.*

That is a direct, prescient caution about Part 7 (coalitions) and Part 8 (treaty
obligations). It's the same worry I raised about exploitability, written years
earlier by someone who'd watched it happen. Design accordingly: tribute priced
against **received** benefit, escrow over reputation, and every new clause
disabled by default via `clause_info.enabled`.

**The ferry system's own suggestion** — dynamically choosing whether boats or
passengers do the searching depending on which is scarcer — is a cheaper
intermediate step than PRD §4.11's full persistent-reservation rewrite, and worth
trying first as a one-turn experiment.

---

## 5. PRD amendments

Add to **§4.5 (Campaigns)** as prerequisites:
- **§2.1** incoming-defence accounting — *before* campaigns, or drained cities
  will fight the campaign with emergency-priority requisitions
- **§2.5** `killstack` gating and a compounding combat model — `required_attack`
  inherits `kill_desire`'s assumptions
- **§2.8** superlinear danger aggregation — same function as own-concentration
  valuation
- **§2.4** "no viable path for N turns" as a campaign abort condition
- **§2.7** allied overseas cities as beachhead candidates

Add to **§4.10 (Geography and allies)**:
- **§2.3** ocean-size gate on shore defence (~5 lines, data already computed)

Add a small new workstream, **§4.15 — Emergency arbitration** (§2.2), sitting
between §4.9 (CM parameters) and §4.10. It's small, self-contained, and improves
play immediately.

Add to **Appendix A (upstream bugs)**:

15. `assess_defense_backend()` counts only units on the city tile; en-route
    defenders are invisible, causing over-requisition. The equivalent accounting
    exists for enemy cities (`ai_invasion`).
16. Shore-defence want triggers on any adjacent oceanic tile including a
    one-tile lake; `adv->oceans[].size` is computed every phase and never read.
17. `kill_desire()` multiplies benefit by `victim_count` with no
    `game.info.killstack` check.
18. `total_danger` aggregates linearly while `assess_defense_quadratic`
    aggregates superlinearly — the AI under-fears enemy concentration.
19. Emergencies are resolved per-city in list order with no arbitration or
    shared budget.
20. **Doc:** `README.AI` still lists `locally_zero_minimap` (identifier removed)
    and describes `military_amortize` as *"a complete mystery"* though it is now
    documented and straightforward.
