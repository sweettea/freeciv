# Part 4 — Expansion geometry and allied logistics

Companion to Parts 1–3. Same tree: `freeciv/freeciv` @ `0896b34`.

---

## 1. Expansion o'clock: the distance discount is the whole story

You guessed the AI smallpoxes outward with no border-staking. It's worse than
that — border-staking isn't underweighted, it's **arithmetically unreachable**.

`settler_map_iterate()` — `daisettler.c:820`:

```c
turns = move_cost / parameter->move_rate;
cr->result = amortize(cr->total, PERFECTION * turns);
```

`PERFECTION = 3` (`daisettler.c:104`), and `amortize()` discounts by
`(MORT-1)/MORT` per unit delay with `MORT = 24` (`advtools.h:19`). So the
per-turn-of-travel factor is `(23/24)^3 ≈ 0.8802`:

| Travel | Discount factor | Site must be _this_ much better to win |
|---:|---:|---:|
| 1 turn | 0.880 | 1.14× |
| 3 turns | 0.682 | 1.47× |
| 5 turns | 0.528 | 1.89× |
| 8 turns | 0.360 | 2.78× |
| 10 turns | 0.279 | **3.59×** |
| 15 turns | 0.147 | 6.79× |
| 20 turns | 0.078 | **12.85×** |

Now compare against the *actual* spread in site quality. Because
`cityresult_fill()` scores essentially two tiles (Part 1 §1a), realistic site
totals across decent land vary by maybe 2–3×. A genuinely spectacular site might
hit 4×.

**So nothing beyond about 8 turns' walk can ever be chosen, at any quality.**
The discount curve crosses the quality ceiling around there and the far map
becomes invisible. That's not a tuning issue; it's a structural guarantee of
oil-spill expansion.

And it doesn't even *look*, because of the early break at `daisettler.c:857`:

```c
if (best && best->result > RESULT_IS_ENOUGH   /* == 250 */
    && turns > parameter->move_rate
    && best_turn < turns) {
  break;
}
```

Find something decent nearby, stop searching. The frontier is never evaluated.

### 1a. There is no territory term at all

Worth being explicit: `cityresult_fill()` scores food, shield, trade,
`result_defense_bonus()`, and `naval_bonus()`. That's it. Nothing values

- claiming unowned tiles,
- denying a site to a rival,
- or securing a chokepoint / land bridge.

The only border-aware line in the whole file is `daisettler.c:813`, and it's a
*negative* — skip tiles owned by players you're at peace with. The AI knows how
to avoid provoking neighbours and has no idea how to beat them to land.

### 1b. But border-first isn't unconditionally better

Here's where I'd push back slightly on the intuition. The distance discount is
modelling something **real**:

- A settler walking 10 turns is 10 turns of a city not existing, compounding.
- Early frontier cities are undefended, far from relief, and barbarian bait.
- There are no roads yet, so "10 turns" is a genuine 10 turns.
- Distance corruption in most rulesets is brutal before Courthouse.

So picket-then-infill is right **when land is contested** and wrong when it
isn't. If you're alone on a continent, nearest-first is correct — the land isn't
going anywhere, and every turn of settler walk is pure loss. The AI doesn't need
a doctrine flip; it needs a **contest signal**.

### 1c. What to build

Two changes, both inside the site evaluator.

**(i) A claim term, scaled by contest pressure.**

```c
/* How many currently-unowned tiles would this city's border claim secure? */
int claimable = count_unclaimed_in_border_radius(ptile, pplayer);

/* How urgently is this land contested? */
float pressure = contest_pressure(pplayer, ptile);
  /* rises with: rival settlers visible, rival cities near, low land remaining,
     shared continent; falls to ~0 when alone on a landmass */

cr->total += claimable * CLAIM_WEIGHTING * pressure;
```

Add a **denial** component: if a rival founds here instead, you lose the whole
catchment, not just this site. Under high pressure the value of a frontier site
is roughly `own_value + denial_value`, which is what makes racing for it
rational.

**(ii) Make the discount adaptive.**

`PERFECTION` is currently a compile-time 3. Make the exponent a function of
contest pressure:

```c
effective_perfection = PERFECTION * (1.0 - 0.6 * pressure);  /* 3.0 → 1.2 */
cr->result = amortize(cr->total, effective_perfection * turns);
```

At `pressure = 1.0` the per-turn factor moves from 0.880 to ≈0.951, so the
10-turn requirement drops from 3.59× to 1.63× — far land becomes reachable
exactly when the race matters. When alone, the current behaviour is preserved,
which is correct.

Also raise `RESULT_IS_ENOUGH` (or gate the early break on `pressure < 0.3`), or
the search will still stop before it sees the frontier.

### 1d. The exploration coupling nobody has wired

Border-staking needs to know where the border *is*, and the AI's explorer and
settler don't talk. `server/advisors/autoexplorer.c` maximises generic tile
discovery; `find_best_city_placement()` searches the known map (with `H_MAP`
gating what counts as known). Nothing directs exploration toward contested
frontiers.

For picket-then-infill you want **directed exploration**: bias the explorer's
desirability function toward the direction of highest contest pressure and
toward unowned land adjacent to known rival territory. That's a small change to
the explorer's want function, and it's a prerequisite — you can't stake a border
you haven't seen.

### 1e. Suggested phase structure

This maps onto the economy axis from Part 3:

| Phase | Trigger | Behaviour |
|---|---|---|
| **Found** | T0 | Capital on the spot. Current behaviour is fine. |
| **Scout** | until frontier shape known | Directed exploration (§1d); settlers take near sites meanwhile |
| **Picket** | `pressure > threshold` | Claim term + flattened discount; take frontier sites at real cost |
| **Infill** | frontier secured, or `pressure` low | Current nearest-first, now filling *inside* claimed land |
| **Consolidate / Enrich** | Part 2 §2b crossover | Stop founding; see Part 3 §2 |

The nice property: infill sites inside your own borders are safe, close, and
road-connected by then, so the existing distance-discounted greedy search is
actually the *right* algorithm for that phase. You're not replacing it, you're
adding a phase in front of it.

Note the interaction with `citymap_is_reserved()` and
`game.info.citymindist` — under CONSOLIDATE you widened the spacing (Part 3),
which means picket cities should be placed at *catchment* spacing, not minimum
spacing, or you'll have staked a border you then can't fill properly.

---

## 2. Allied forward bases

This one has a genuinely delightful finding: **most of what you want already
works, and the AI simply doesn't know.**

### 2a. Healing in allied cities is free and unused

`unit_gain_hitpoints()` — `common/unit.c`:

```c
struct city *pcity = tile_city(unit_tile(punit));
...
if (pcity == nullptr && !tile_has_native_base(unit_tile(punit), utype)
    && !unit_transported(punit)) {
  gain -= utype->hp * pclass->hp_loss_pct / 100;
}
```

**No owner check.** Being in *any* city — including an ally's — avoids field
attrition entirely.

Better, civ2civ3's barracks effect:

```
[effect_barracks_regen]
type    = "HP_Regen"
value   = 100
reqs    =
    { "type",          "name",     "range", "present"
      "BuildingFlag",  "Barracks", "City",  TRUE
      "UnitClassFlag", "Barracks", "Local", TRUE
    }
```

No `Player` or `DiplRel` requirement. The `City` range resolves against the city
at the unit's tile regardless of who owns it. **A damaged unit parked in an
allied city with Barracks heals to full in one turn, today, with no ruleset
change and no treaty.**

The AI never does this. `dai_military_findjob` and the recovery logic route
damaged units to *own* cities only.

**Fix:** in the unit task that handles recovery, search allied cities too, and
score them by `hp_regen_available × 1/distance`. Pure pathfinding change, no
diplomacy required. This is the cheapest item in any of these four documents.

### 2b. Airlift is own-cities-only and purely defensive

`dai_airlift()` — `daiunit.c:168`, with `find_neediest_airlift_city()` at 137:

```c
city_list_iterate(pplayer->cities, pcity) {   /* only our own */
  if (pcity->airlift) {
    if (city_data->urgency > most_urgent) { ... }
    else if (0 == most_urgent && city_data->danger > most_danger) { ... }
```

Three limitations, all fixable:

1. **Own cities only** — never considers allied cities as source or destination,
   even though `AIRLIFTING_ALLIED_SRC` (`FROM_ALLIES`) and
   `AIRLIFTING_ALLIED_DEST` (`TO_ALLIES`) exist as `airliftingstyle` bits
   (`server/settings.c:549–551`). Default is `AIRLIFTING_CLASSICAL`, so allied
   airlift is usually off — but when a server enables it, the AI still can't use it.
2. **Defensive only** — it moves units *toward* danger and urgency. There is no
   offensive airlift at all.
3. **It only moves attackers** — `IS_ATTACKER(ptype) && DEFENSE_POWER(ptype) > 2`,
   i.e. it pulls your good attackers home to garrison duty. Combined with (2),
   airlift currently *drains* offensive capacity.

**Fix:** wire airlift into the campaign object from Part 1 §3. During
`CAMP_FORMING`, airlift is the fastest way to fill `required_attack -
committed_attack` — you skip the whole walk. Score airlift destinations by
campaign need, not just danger, and include allied cities when the
`airliftingstyle` bits permit. Check `game.info.airlifting_style` in
`phase_begin` and cache what's legal.

### 2c. Asking an ally for a city

The AI's entire initiating diplomatic vocabulary — every
`dai_diplomacy_suggest()` call in `daidiplomacy.c` — is:

`CEASEFIRE`, `PEACE`, `ALLIANCE`, `ADVANCE`, `GOLD`, `VISION`, `EMBASSY`.

**`CLAUSE_CITY` is never proposed.** It's evaluated when *offered*
(`daidiplomacy.c:511`), so the machinery exists — the AI just has no concept of
asking for territory, or of giving it.

To implement a forward-base request:

1. **Value the city as a base**, not as an economy asset. The existing
   `CLAUSE_CITY` valuation at line 511 is economic. A base wants: distance to
   the campaign target, Barracks/Airport present, coastal for naval staging,
   and whether it shortens `move_time` in `find_something_to_kill`.
2. **Construct an offer.** `dai_goldequiv_clause()` gives you the *ally's*
   valuation of gold and tech, so you can price the request. This is the same
   machinery `DIP_SUE` needs (Part 3 §4) — build it once.
3. **Gate on alliance depth.** Only ask an ally whose love exceeds
   `req_love_for_alliance` by a margin, and respect a spam limiter like the
   existing `asked_about_*` counters.

**Two cautions.** First, city-gifting between AIs is exploitable and can cascade
— an AI that will trade cities for gold is an AI a human will farm. Gate it hard
and consider making it alliance-only and campaign-conditional.

Second, and this cross-references Part 2 §3c: a city received from an ally is
full of *their* nationals, who migrate back toward their own nation's cities at
**2× attractiveness** (`cityturn.c:4718`). A gifted forward base will bleed
population back to the giver unless you can out-score them. Model that before
paying for it — the base may be a leaky asset, which is a fine trade for a
staging point in a 20-turn campaign and a bad one as a permanent holding.

### 2d. Priority

Tier by cost:

- **Tier 1 — allied healing (§2a).** Works today, zero diplomacy, no ruleset
  change, small pathfinding change. Do it first.
- **Tier 2 — allied territory as campaign pathing.** Allied units may already
  move through allied territory and cities; the campaign planner should use them
  as waypoints and staging tiles. Also nearly free.
- **Tier 3 — offensive airlift (§2b).** Moderate; depends on the campaign object.
- **Tier 4 — requesting cities (§2c).** Real diplomacy work, shares
  infrastructure with `DIP_SUE`, and carries the exploit and backflow risks
  above. Last.
