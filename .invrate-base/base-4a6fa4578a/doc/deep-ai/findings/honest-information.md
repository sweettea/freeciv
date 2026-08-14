# Part 11 — Honest information

Companion to Parts 1–10 and the PRD. Same tree: `freeciv/freeciv` @ `0896b34`.

**This is a PRD amendment, not an addendum.** It changes the difficulty model in
Part V and adds a workstream that several others now depend on.

---

## 1. What the AI actually knows

Three tiers, and they get worse as you go up.

### Tier 1 — Tactical: gated, but the gate is open at every real difficulty

There are ~40 sites of the form:

```c
parameter.omniscience = !has_handicap(pplayer, H_MAP);
```

across pathfinding, targeting, danger assessment, exploration, worker search,
ferries, air units, diplomats and hunters. Plus scattered
`has_handicap(pplayer, H_FOG)` and `H_TARGETS` checks.

So the plumbing exists. Now look at `ai/difficulty.c:52–133`, which assigns
handicaps per level:

| Level | Information handicaps |
|---|---|
| `AWAY` | `H_MAP`, `H_FOG`, `H_TARGETS`, `H_HUTS` |
| `NOVICE` / `RESTRICTED` | `H_TARGETS`, `H_HUTS` |
| `EASY` | `H_TARGETS`, `H_HUTS` |
| `NORMAL` | `H_TARGETS`, `H_HUTS` |
| `HARD` | **none** (only `H_RATES`) |
| `CHEATING` | **none** (only `H_RATES`) |
| `EXPERIMENTAL` | **none** |

**`H_MAP` and `H_FOG` are set at exactly one level — `AWAY`**, the "I'm not
playing this civ" mode. Every level anyone actually plays against sees the entire
map and through all fog. At `hard`, the AI has no information handicap at all.

And note the framing: omniscience is the **default**, and blindness is a
*handicap* applied to make the AI weaker. That's backwards from a fairness
standpoint, and it's why "make the AI honest" reads as "make the AI worse" in
this codebase.

### Tier 2 — The AI never consults remembered state

```
$ grep -rn "map_get_player_tile\|map_get_player_city" ai/ server/advisors/
(nothing)
```

The AI reads **ground truth** everywhere: `tile_terrain(ptile)`,
`tile_city(ptile)`, `aplayer->cities`, `aplayer->units`. The `H_MAP`/`H_TARGETS`
checks are spot patches layered on top of that — `find_something_to_kill()`
iterates the *real* `aplayer->cities` list and merely skips entries failing
`map_is_known()`. It still knows the city exists and where it is; it just
declines to target it.

There is no concept of "I remember a city here, size 6, seen 20 turns ago."

### Tier 3 — The strategic layer is unconditionally omniscient

This is the one that surprised me. Handicap-check counts:

| File | `has_handicap` calls | Which |
|---|---:|---|
| `daidiplomacy.c` | 8 | `H_AWAY`, `H_DIPLOMACY`, `H_CEASEFIRE` — **no information handicaps** |
| `daitech.c` | **0** | — |
| `daidata.c` | **0** | — |

`dai_war_desire()` (`daidiplomacy.c:797`) — the function that decides whether to
go to war — does this, with no gating whatsoever:

```c
city_list_iterate(target->cities, pcity) {
  want += city_size_get(pcity) * 20;
  want += pcity->surplus[O_SHIELD] * 8;
  want += pcity->surplus[O_TRADE] * 6;
  fear += get_city_bonus(pcity, EFT_DEFEND_BONUS);
  city_built_iterate(pcity, pimprove) { want += cost; ... }
}
unit_list_iterate(target->units, punit) { fear += ATTACK_POWER(ptype); }
fear += (target->economic.gold / 5000) * city_list_size(target->cities);
fear += MAX(research_get(target)->techs_researched - ours, 0) * 100;
```

Every enemy city's exact size, shield surplus, trade surplus and complete
building list. Every enemy unit's type and location. Their treasury. Their exact
tech count. **At every difficulty level, including `AWAY`.**

The information handicaps only ever touched tactics. The AI's decision to
declare war on you has always been made with a perfect census.

---

## 2. Why this matters more than fairness

Three arguments, in ascending order of importance.

**It's more fun.** An honest AI can be *deceived*. Feints, hidden stacks, false
buildups, a defenceless-looking border that is a trap — none of that currently
exists as gameplay, because the AI cannot be fooled. Adding honest information
adds an entire dimension of play that no amount of AI strength can substitute
for.

**It's better AI design.** Handicapping information makes an AI behave
*arbitrarily* — it walks into an ambush for no visible reason and the player
learns nothing. Honest information plus scaled competence makes it behave like a
*weaker player*, which is legible and teachable. This supersedes Part V of the
PRD: don't scale information at all, scale competence at every level.

**It's a measurement prerequisite, and this is the real argument.** Right now you
cannot tell whether the Freeciv AI is *good* or merely *informed*. Every
improvement in this project — doctrine, campaigns, road planning, migration
intel — is meant to be a strategy layer, and you cannot evaluate a strategy
layer that is being fed answers. If you build all of Parts 1–10 on top of
omniscience, you will never know which parts worked.

**Make it honest early or the whole project is unfalsifiable.**

---

## 3. The good news: the memory substrate already exists

The server already maintains a complete per-player remembered world — it's what
the client renders as fogged tiles, and it's already in savegames.

`server/maphand.h`:

```c
struct player_tile {
  struct vision_site *site;      /* remembered city, NULL if none */
  struct extra_type  *resource;
  struct terrain     *terrain;   /* NULL for unknown */
  struct player      *owner;
  struct dbv          extras;
  short               last_updated;   /* <-- staleness, free */
};
```

`common/vision.h`:

```c
struct vision_site {
  struct tile   *location;
  struct player *owner;
  citizens       size;
  bool           occupied;
  int            walls;
  bool           happy, unhappy;
  enum capital_type capital;
  bv_imprs       improvements;
};
```

Remembered terrain, resources, ownership, extras, plus remembered city size,
garrison presence, walls, improvements, and capital status — **with an age
stamp**. Accessors are `map_get_player_tile()` and `map_get_player_city()`.

The AI has never called either.

So the honest-information workstream is not "build a fog-of-war model." It's
**"read the fog-of-war model that has been sitting there the whole time."**

---

## 4. The hard part: removing omniscience without adding inference makes it worse

If you simply set `H_MAP`/`H_FOG`/`H_TARGETS` at all levels, the AI gets *worse*
in ways that are not "fairer," they're just broken:

| Subsystem | What breaks |
|---|---|
| **Settling** | `city_desirability()` returns NULL for unknown tiles (`daisettler.c:724`). With **no explorer production want at all** (Part 10 §1) and an active disband-explorers hack, the AI would stop expanding almost immediately. |
| **Danger** | `adv_danger_at()` only counts enemies on `map_is_known_and_seen` tiles. Without remembered threats it walks into the same ambush every turn, having forgotten. |
| **Targeting** | `find_something_to_kill()` skips unknown cities, and nothing generates reconnaissance to find them. |
| **War decisions** | `dai_war_desire` would need estimates it has no machinery to produce. |

An honest AI needs **inference** where it used to have answers. That's four
subsystems, and they're genuinely the interesting part — this is what makes an AI
feel like it's thinking rather than looking things up.

---

## 5. Design: a knowledge layer

### 5.1 Remembered world model

```c
/* Per enemy player, maintained each phase from map_get_player_* */
struct known_city {
  struct tile *location;
  int   size, walls;
  bool  occupied, is_capital;
  bv_imprs improvements;
  int   last_seen_turn;
  int   est_size, est_defence;   /* projected forward from last_seen */
};

struct known_force {
  struct tile *last_seen_tile;
  const struct unit_type *utype;
  int   last_seen_turn;
  float position_confidence;     /* decays with age and their move rate */
};
```

Build from `map_get_player_city()` and `map_get_player_tile()` each phase — cheap,
and it replaces iterating `aplayer->cities` wholesale.

### 5.2 Staleness projection — the part that reads as intelligence

`last_updated` gives you age for free. Project forward rather than assuming the
snapshot is current:

- **City size**: grow at an estimated rate since last seen, capped by the
  aqueduct/sewer tech you know they have.
- **Defence**: assume they've built the best defender their known tech allows,
  and add garrison over elapsed turns proportional to their estimated production.
  `dai_choose_defender_versus()` (`daimilitary.c:89`) already does this
  projection for the *current* turn — extend it over elapsed time.
- **Unit position**: confidence decays as a function of age × their move rate.
  A stack seen 10 turns ago could be anywhere within 10 × move_rate.

This is exactly how a human reasons — *"I haven't looked at Berlin in thirty
turns; assume it's bigger and better defended than I remember"* — and it produces
the single most human-feeling behaviour an AI can have: **appropriate caution
about things it hasn't checked recently.**

### 5.3 Reconnaissance as a first-class want

Once information is scarce it acquires value, and the AI must *choose* to buy it.

```c
recon_want(target) = intel_staleness(target)
                   * planned_action_value(target)    /* campaign? settle? */
                   * (1 / distance)
```

Consequences that fall straight out:

- **A campaign must scout before it commits.** `required_attack` depends on
  observing the defender. `CAMP_FORMING` gains a precondition: intel on the
  target newer than N turns. That is a *dramatically* better behaviour than
  today's "know everything, attack anyway."
- **The explorer want from Part 10 §1.4 stops being optional** — it becomes
  load-bearing, and the disband hack must go.
- **Embassies become strategic.** An embassy legitimately reveals gold and tech.
  `CLAUSE_EMBASSY` is already proposed by the AI (`daidiplomacy.c:1290`) but for
  no particular reason; under honest information it's the cheapest intel purchase
  in the game, and the AI should want it *before* evaluating war.
- **Diplomats/spies acquire a real role** beyond incite and steal —
  investigating a city is direct intel on `required_attack`.

### 5.4 Priors over the unknown

Unexplored territory isn't worth zero, and a human doesn't treat it that way. The
AI legitimately knows the map generator settings, map size, player count,
`game.server.huts` density, and landmass parameters. So:

```c
expected_value(unknown_tile) = P(land) * mean_tile_value
                             + hut_density * expected_hut_value
                             + P(unclaimed) * claim_value   /* Part 4 §1c */
```

This is what lets an honest AI still expand sensibly: it settles toward
*expected* good land, discovers, and updates. It also gives directed exploration
(Part 4 §1d) a principled objective function rather than a heuristic.

### 5.5 Honest `dai_war_desire`

Rebuild the census from `known_city` and `known_force` with staleness projection,
and add an **uncertainty penalty**: the less you know about a target, the wider
your error bars, and a rational actor discounts uncertain gains while inflating
uncertain threats. Concretely, scale `want` down and `fear` up by intel age.

That single change produces a behaviour the current AI cannot express: **the AI
becomes reluctant to attack a neighbour it hasn't scouted**, which is both
correct play and immediately legible to a human.

Keep the legitimate channels honest too: tech count is knowable *with an
embassy*; treasury likewise. Without one, estimate from city count and elapsed
time. That makes embassies matter and makes `CLAUSE_EMBASSY` refusal a
meaningful defensive move.

---

## 6. Difficulty, reframed

This replaces the difficulty model in PRD Part V.

**Every level gets honest information.** Difficulty scales competence:

| Knob | Novice | Normal | Hard |
|---|---|---|---|
| Planning horizon `H` | 10 turns | 25 | 40 |
| Recalculation cadence | slow | medium | event-driven |
| Campaign force margin | 1.1× (attacks under-strength) | 1.5× | 2.0× |
| Coordination fidelity | 50% of units follow the plan | 80% | 100% |
| Recon before commit | rarely | sometimes | always |
| Staleness tolerance | acts on 40-turn-old intel | 20 | 8 |
| Doctrine probe cadence | 60 turns | 35 | 20 |

Every row degrades gracefully into behaviour that reads as *a weaker player*
rather than *a blind one*. A novice AI attacks with too few units on stale
information — which is exactly what a novice human does. That is far better
game design than an AI that inexplicably fails to notice an army.

Keep `H_MAP`/`H_FOG` in the enum for `AI_LEVEL_AWAY` and for compatibility, and
keep `CHEATING` as an explicit, honestly-named level that restores omniscience
plus ruleset bonuses — players who want that should be able to ask for it.

---

## 7. Cost, and one pleasant surprise

**Cheaper in places.** `find_something_to_kill()` currently iterates *every*
enemy city on the map for *every* attacking unit. Restricted to remembered cities
that's a much smaller set — and combined with the campaign hoisting from PRD
§4.5, target evaluation drops by orders of magnitude.

**More expensive in others.** Maintaining `known_city`/`known_force` per enemy per
phase, and reconnaissance planning, are new costs. Both are O(known things), not
O(map), and both are pure functions of state — so both are **threadable** under
PRD §6.2.

**Savegame:** the substrate is already persisted by the server. Only your derived
estimates need saving, and per PRD §6.4's rule they're recomputable from
`player_tile` — so save nothing and rebuild on load.

---

## 8. Where it goes in the roadmap

It's a **P0-adjacent** dependency, not a nice-to-have, because of §2's third
argument: build the strategy layer on omniscience and you can't evaluate it.

| Revised phase | Contents |
|---|---|
| **P0.0** | + add an `honest_information` flag; run the existing `H_MAP`/`H_FOG`/`H_TARGETS` paths at all levels in a test config and **record what breaks** |
| **P0.3** | Knowledge layer §5.1–5.2 (remembered world + staleness). Cheap: read existing structures. |
| **P1.1** | Exploration (Part 10 §1) becomes **blocking**, not optional — honest settling needs it |
| **P1.5** | Campaigns gain a recon precondition (§5.3) |
| **P1.6** | Honest `dai_war_desire` (§5.5) |
| **P2.x** | Priors over unknown (§5.4); competence-scaled difficulty (§6) |

The sequencing point: **turn on the handicaps first, in a test config, and watch
it fall over.** The failure list *is* the requirements document for the knowledge
layer, and it's a half-day of work to generate.

---

## 9. Upstream bug candidates (additions to PRD Appendix A)

11. **`dai_war_desire()` performs a perfect census of enemy cities, buildings,
    units, gold and tech with zero handicap checks**, at every difficulty
    including `AI_LEVEL_AWAY` where `H_MAP` and `H_FOG` are set. The information
    handicaps are documented as limiting AI knowledge; the strategic layer
    ignores them entirely.
12. **`daitech.c` and `daidata.c` contain no handicap checks at all.**
13. **`H_MAP` and `H_FOG` are only ever set at `AI_LEVEL_AWAY`** — no
    player-facing difficulty level restricts AI map knowledge or fog vision.
14. **The AI never reads `map_get_player_tile()` / `map_get_player_city()`**,
    so the server-maintained per-player knowledge model is unused by the only
    consumer that would need it.
