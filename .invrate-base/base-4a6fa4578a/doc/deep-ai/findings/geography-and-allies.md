# Part 5 — Geographic and allied awareness: an audit

Companion to Parts 1–4. Same tree: `freeciv/freeciv` @ `0896b34`.

---

## 0. Correction to Part 4 §2a

**I was wrong.** I said the AI never heals in allied cities. It does.

`find_nearest_safe_city()` — `daiunit.c:1713`, docstring:

> *Find safe city to recover in. An allied player's city is just as good as
> one of our own, since both replenish our hitpoints and reduce unhappiness.*

and the filter at line 1733:

```c
pcity = tile_city(ptile);
if (pcity == nullptr || !pplayers_allied(pplayer, city_owner(pcity))) {
  continue;
}
```

It even checks HP regen at the destination and penalises cities that lack it:

```c
if (0 == get_unittype_bonus(unit_owner(punit), ptile,
                            unit_type_get(punit), nullptr, EFT_HP_REGEN)) {
  cur *= 3;   /* If we cannot regen fast our hit points here, penalise */
}
```

So the barracks-has-no-owner-requirement finding was correct, but the conclusion
drawn from it wasn't — the AI already exploits it. Scratch "cheapest item in any
of these documents."

What remains true, and is a smaller claim: recovery is **reactive only**. It
fires when `AIUNIT_RECOVER` is set (`daiunit.c:976`) because a unit is already
badly hurt. Allied cities are never used as *forward staging* — chosen in
advance as a campaign muster point, or garrisoned to keep them available. That's
still worth building, it's just an enhancement rather than a gap.

---

## 1. Defensibility of borders

**No.** And there's a bug underneath.

The only defence term in site scoring is `result_defense_bonus()`
(`daisettler.c:596`), which is about the **terrain defence of the single city
tile** — hills, forest, river, fortress. Nothing about frontage, natural
barriers, defensive lines, or how exposed a border is.

### The bug

```c
int defense_bonus = 10 + tile_terrain(result->tile)->defense_bonus / 10;
...
return 100 / (result->total + 1) * (100 / defense_bonus * DEFENSE_EMPHASIS);
```

`100 / defense_bonus` is **integer division between two ints**, and
`defense_bonus` *grows* with terrain defence. So the term shrinks as the site
gets more defensible. Working it through with civ2civ3 terrain values
(`DEFENSE_EMPHASIS = 20`, site total 500):

| Terrain | `defense_bonus` in ruleset | computed term |
|---|---:|---:|
| Grassland | 0 | **39.9** |
| Forest | 25 | 31.9 |
| Hills | 50 | 23.9 |
| Mountains | 100 | **20.0** |

**The AI is penalised for settling defensible terrain**, by a factor of two
between grassland and mountains.

It's also inversely proportional to `result->total`, so it matters *more* on bad
sites than good ones.

Compare the function immediately below it, `naval_bonus()`, which does it the
way you'd expect:

```c
return (result->total * NAVAL_EMPHASIS) / 100;
```

And the docstring of the broken one says:

> *Calculate defense bonus, which is a % of total results equal to a given % of
> the defense bonus %.*

That describes something proportional to both `total` and `defense_bonus`. The
code is inversely proportional to both. **The implementation contradicts its own
documentation**, which makes this reportable upstream independent of your AI
work. Likely intended:

```c
return (result->total * defense_bonus * DEFENSE_EMPHASIS) / 10000;
```

### What real border defensibility would need

A per-site term over the *border* the city would claim, not the tile it sits on:

- **frontage**: length of the claimed border adjacent to unclaimed or hostile
  territory, normalised by area gained. Low frontage-per-area = defensible.
- **barrier fraction**: how much of that frontage is ocean, mountain, or
  otherwise impassable/slow to the enemy's dominant unit classes.
- **mutual support**: can adjacent friendly cities reinforce this one within
  1–2 turns? (You have the road graph from Part 1 §2 by then.)

---

## 2. "At least one ocean city"

**No — it's per-site, never empire-level.**

`naval_bonus()` (`daisettler.c:625`) gives *every* coastal site a flat
`total × 20 / 100`. The fifth port gets the same 20% as the first. There is no
check anywhere for "do we have any coastal city at all", "can we build boats",
or "are we about to be landlocked".

The consequence is exactly the failure mode you're pointing at: if inland sites
happen to out-score coastal ones, the AI can end up with no port, no navy, no
ferries, and no overseas anything — and nothing in the code notices. Note
`adv->stats.boats` / `available_boats` exist (`daidata.h:76`) but only count
boats you already have; nothing tracks the *capability* to build them.

**Fix:** a step-function empire-level want. If `count_coastal_cities() == 0`,
add a large flat bonus to any coastal site (and a corresponding want for a
Harbour once you have one); once you have 1–2, decay the per-site naval bonus
toward its current value. Same shape as the deterrence "stopping point" from
Part 3 §3 — a need that's satisfiable rather than a permanent multiplier.

---

## 3. Chokepoints

**Nothing.** `grep -rni "chokepoint|choke|isthmus|bottleneck|land.bridge"` across
`ai/` and `server/advisors/` returns two hits, and both are unrelated. One is a
comment about effect ranges. The other, `daisettler.c:809`, is the AI
*discarding* the concept:

```c
/* We have an accidental land bridge. Ignore it. It will in all
 * likelihood go away next turn, or even in a few nanoseconds. */
```

No terrain graph, no articulation-point analysis, no notion that one tile might
control access to a whole region.

**What you'd build:** treat the passable-terrain map as a graph and find
articulation points / minimum cuts between your territory and each rival's. A
tile whose removal disconnects a large region is a chokepoint, and its value is
roughly `size_of_region_protected × contest_pressure` (reusing the pressure
signal from Part 4 §1c). That gives you both halves of what you asked:

- **Defensive**: a city or fortress on a chokepoint protects everything behind
  it, which is a *border defensibility* term (§1) computed properly.
- **Shortcut**: the same analysis identifies where a road/rail through the
  chokepoint collapses travel time between two regions — feed it into the road
  planner from Part 1 §2 as a high-priority trunk edge.

This is the most genuinely novel item across all five documents; no Freeciv AI
has ever done topological analysis of the map.

---

## 4. Consolidating the home continent once safe

**The data exists and nothing strategic reads it.**

`struct adv_area_info { int size; bool threat; }` — `advdata.h:56`. Set at
`advdata.c:317–331`: `threat = TRUE` if any player you consider dangerous has
*any* city on that continent.

Two problems:

1. It's a **boolean**, not a magnitude. One hostile size-2 town on your
   continent reads identically to a neighbour with thirty cities and a stack of
   armour.
2. It's consulted in exactly **three places**, all minor: `daieffects.c:541,548`
   (whether walls/coastal defence are worth building) and `advbuilding.c:241`
   (wonder city gets −25% if its continent is threatened).

The settler never reads it. The campaign logic never reads it. There is no
"continent is clear → shift to consolidation" trigger anywhere.

**Fix:** make it a magnitude (`threat_level` = weighted hostile power and city
count on that continent, decayed by distance), and feed it into the doctrine
probe from Part 3 as one of the inputs that moves the economy axis from
`ECON_LANDGRAB` toward `ECON_CONSOLIDATE`. "Home continent secured" is exactly
the condition under which consolidation becomes correct, and it's currently
invisible.

---

## 5. Settling a safe empty continent when home is threatened

**No safety term at all.**

`find_best_city_placement()` (`daisettler.c:896`) runs Phase 1 on your own
continent, Phase 2 overseas, and picks whichever `cr->result` is larger. The
overseas candidate is penalised twice — by `cost + boat_cost / 3`
(`daisettler.c:844`) and by the same brutal distance discount from Part 4 §1
applied to sailing turns.

But `adv->continents[].threat` never enters the comparison. A home continent
crawling with hostiles does **not** make a safe empty continent more attractive.
The decision is purely "which site scores higher after travel penalty," and
since overseas always costs more travel, the answer is nearly always "stay home."

**Fix:** apply a safety differential to the Phase 1 vs Phase 2 comparison:

```c
/* Sites on a low-threat continent are worth more when home is contested */
float safety_mult = 1.0 + SAFETY_EMPHASIS
                    * (home_threat_level - dest_threat_level);
cr2->result *= MAX(safety_mult, 0.5);
```

This is also the natural home for a "refuge" behaviour — when
`home_threat_level` is extreme and you're losing, an empty continent is worth
far more than its raw tile yield, because it's survival. That interacts well
with `DIP_SUE` from Part 3 §4: sue for peace *and* evacuate.

---

## 6. Allies: defending, building, gifting

### 6a. Defending allies — actively refuses

`dai_military_defend()` — `daiunit.c:1015`:

```c
struct city *pcity = aiguard_charge_city(ait, punit);
if (!pcity || city_owner(pcity) != pplayer) {
  pcity = tile_city(unit_tile(punit));
  /* Do not stay defending an allied city forever */
  aiguard_clear_charge(ait, punit);
}
```

If the city it's guarding isn't its own, it drops the assignment. And
`daiunit.c:853` carries the explicit TODO:

```c
/* City bodyguard. TODO: allied city bodyguard? */
```

`assess_danger()` and `dai_assess_danger_player()` (`daimilitary.c:709, 643`)
iterate own cities only, so an ally's city under siege generates no danger
signal at all. The AI cannot see that an ally is in trouble, let alone respond.

### 6b. Building infrastructure for allies — no

`autoworker_tile_behavior()` (`autoworkers.c:411`) lets workers **path through**
allied territory:

```c
if (NULL != owner && !pplayers_allied(owner, param->owner)) {
  return TB_IGNORE;
}
```

But the work *search* is `city_list_iterate(pplayer->cities)` — your own cities
only (Part 1 §2). So a worker will happily walk across an ally's land and will
never improve a tile on it. No joint road network, no shared rail trunk.

This one is nearly free once you've built the road planner: include allied
cities as nodes in the MST when the alliance is stable, and let the road-plan
tile list include allied territory. Their roads are your roads for movement
purposes.

### 6c. Donating tech to allies — team-mates only

`dai_share()` — `daidiplomacy.c:1248`:

```c
/* Only share techs with team mates */
if (presearch != aresearch
    && players_on_same_team(pplayer, aplayer)) {
  ... dai_diplomacy_suggest(..., CLAUSE_ADVANCE, ...) ...
}
```

An **ally who isn't a team-mate gets nothing** — only shared vision, embassy,
and `suggest_tech_exchange()`, which is a mutual *trade*, not a donation.

Worth flagging because `doc/README.AI` says of alliance:

> *it will happily hand over any tech and maps it has to you*

That describes how the AI *values incoming requests* in `dai_goldequiv_clause`
(it prices things cheaply for allies), not proactive gifting. The AI never
offers. So an ally being overrun receives no defensive tech, ever.

**Fix:** gate proactive gifting on ally need rather than team membership. You
need an ally-distress signal first, which §6a says doesn't exist — so build
that, then: if an ally's threat level is high and they lack a defensive tech you
have (gunpowder, feudalism, whatever the ruleset's defender line is), gift it.
Cheap, high-value, and it keeps a buffer state alive, which is worth more than
the tech.

---

## 7. Summary

| Question | Status |
|---|---|
| Border defensibility | **Absent**, and the per-tile proxy is inverted (§1 — bug) |
| At least one ocean city | Absent; flat 20% per coastal site, no empire-level need |
| Chokepoint awareness | **Entirely absent**; one comment explicitly discards the idea |
| Consolidate home continent when safe | Data exists as a boolean, no strategic consumer |
| Settle safe continent when home threatened | No safety term in the home-vs-overseas comparison |
| Defend allies | **Actively refuses**; explicit TODO; ally danger not even assessed |
| Build infra for allies | No — workers may traverse allied land, never improve it |
| Donate tech to allies | Team-mates only; non-team allies get nothing proactively |
| Heal in allied cities | **Already works** (correction, §0) — reactive only |

The chokepoint work (§3) is the one I'd single out. It's the only item here that
isn't "wire up data that already exists," and it feeds three separate systems —
border defence, the road planner, and campaign target selection.
