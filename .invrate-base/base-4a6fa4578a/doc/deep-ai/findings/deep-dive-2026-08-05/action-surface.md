# Action & unit-capability surface: what the game offers that no AI code exploits (reader report, verbatim)

**Scope:** every entry in the `gen_action` enum (`gen_headers/enums/actions_enums.def:1-156`), cross-checked against `ai/default/`, `ai/deep/`, `server/advisors/`, and both benchmark rulesets. All claims VERIFIED (line read / exact grep) unless marked INFERRED.

---

## 0. The two filters that must be applied before any "prize" claim

**Filter 1 — ruleset.** A large fraction of the action enum is not enabled in *either* ruleset we benchmark, so "the AI never does it" is not an opportunity. VERIFIED by zero-count grep over both `actions.ruleset` files:

`Expel Unit`, `Suitcase Nuke`, `Destroy City`, `Spread Plague`, `Surgical Strike Building`, `Surgical Strike Production`, `Heal Unit`, `Wipe Units`, `Unit Make Homeless`, `Teleport*`, `Steal Maps*`, `Convert Unit` (no `convert_to` field on any unit in either ruleset — `data/civ2civ3/units.ruleset:271-272`, `data/classic/units.ruleset:234-235` are comments only).

Note the consequence: `ai/default/daiactions.c:206` values `ACTRES_STEAL_MAPS` at +3000 and `:211` values `ACTRES_SPY_SPREAD_PLAGUE` at +2500 — **both are dead valuation code in both our venues.**

**Filter 2 — era.** Empirical, from a full 20-player classic game to T162 in the repo root (`freeciv-T0162-Y01110-final.sav.zst`, `rulesetdir="classic"`, `ai_type="deep"`; untracked debug-run leftovers, not benchmark output, so single-sample):

| Unit census (T162) | Extras on map (T162) | Buildings, all 175 cities |
|---|---|---|
| Musketeers 198, Cannon 151, Riflemen 72, **Diplomat 63**, Knights 40, Phalanx 24, Pikemen 22, Barbarian Leader 14, Alpine Troops 13, Ironclad 10, Engineers 10, Galleon 9, Dragoons 9 | River 1112, Road 664, Irrigation 371, Mine 31 | Barracks II 101, City Walls 21, Palace 20, Aqueduct 5, Library 5, Temple 5, Marketplace 1, Bank 1 |
| **Zero** Paratroopers, Bomber, Fighter, Armor, Nuclear, Spy, Marines, Partisan | **Zero Fortress. Zero Airbase. Zero Buoy.** Zero Farmland, zero Railroad | **Zero Airport** |

This corroborates the tech-gate analysis: campaign cells run T80–T151 at ~16–29 techs; Flight is 50 techs, Advanced Flight 57, Paratroopers 60. `doc/deep-ai/STATUS.md:856-861` already flags this as an unclosed action item and asks for exactly this savegame check — **it is now done, and the answer is: the flight-era capability surface is unreachable in every measured cell.**

Also measured in that save: **36 successful tech steals across 175 cities** — the diplomat channel demonstrably fires.

---

## 1. Capability table

### NEVER-TOUCHED, and reachable in our venues — the actual prize list

| Capability | Evidence of zero use | Reachable? |
|---|---|---|
| **Pillage** (non-barbarian) | The *only* pillage in the tree is `ai/default/daiunit.c:930-938` — barbarian-only, unconditional, no target evaluation; plus `daiunit.c:1840` letting barbarians continue. `ai/deep/` has zero pillage references. `server/advisors/autoworkers.c:128`: *"We could have ACTRES_PILLAGE here, but currently we don't"* | Enabled both rulesets (`civ2civ3/actions.ruleset:1077`, `classic:1032`). Available turn 1 |
| **Bases — Fortress/Fort/Airstrip/Airbase/Buoy** | `ACTRES_BASE` *is* in the autoworker action array (`autoworkers.c:121-122`), but a base's want is `adv_calc_extra()` → `city_tile_value()` (`infracache.c:258-283`, `:382-405`), which sums **food/shield/trade only**. A base changes no output ⇒ want is exactly 0; the `extra` bonus term is computed only for roads with a move bonus (`autoworkers.c:772-812`). Empirically **0 fortresses, 0 airbases, 0 buoys** at T162 | Fortress at Construction in both. **civ2civ3 puts the `Fortress` *unit* flag on 16 combat types** (`civ2civ3/units.ruleset:694…1112`) — any Legion can dig a fort — but the only path to base-building is worker-gated (`daisettler.c:1032`, `deepsettler.c:1662`, `autoworkers.c:1122`), so that entire civ2civ3 affordance is unreachable |
| **Capture Units** *(as a chosen act)* | Performed only as the first branch of a fixed else-if chain in `daitools.c:835-839` / `daiunit.c:452-454`, reached *after* the decision to attack. `dai_rampage_want` (`daiunit.c:531-556`) uses the chain solely as a legality gate then prices via `unit_win_chance`. No code seeks a capturable target; nothing protects own capturables (sole "Avoid capture": `daitools.c:558-560`, a Barbarian-Leader ransom guard) | **civ2civ3 only** (`actions.ruleset:681`); 19 Capturer types from Warriors on; targets are Migrants + both caravan types. Reachable turn 1 in the primary ruleset |
| **Fortify as a decision** | `daiunit.c:2694-2713`: terminal fallback keyed on `AIUNIT_DEFEND_HOME`. Never chosen for terrain, never for a chokepoint | Enabled both |
| **Airlift** *(offensively, or at all)* | `dai_airlift` (`daiunit.c:168-212`) purely defensive; `ai/deep` reuses verbatim (`deepops.c:2062`). Never concentrates force. **And it can never fire**: `EFT_AIRLIFT` want is `v += c + MIN(airliftable, 13)` under *"FIXME: …totally braindead"* (`daieffects.c:314-318`) ⇒ **0 Airports in 175 cities** | Airport needs Radio. Marginal at T150, live at T250+ |

### USED-MECHANICALLY — performed, never valued on merit

| Capability | Where | The gap |
|---|---|---|
| Bombard (6 variants) | `daitools.c:845-874`, priority above regular attack | Want is `stack_cost × win-chance` — the value of *killing*. Bombard doesn't kill. **civ2civ3 `attack_blocked_by` means a Bomber cannot regular-attack** — it bombards forever, priced as a kill each pass. INFERRED (strong). Plus `daimilitary.c:606-607` and `:624-625` FIXMEs on anti-bombard defence |
| Collect Ransom | `daitools.c:840-844` | Same chain, same pricing. 14 Barbarian Leaders alive at T162 |
| Nuke family | `daitools.c:875-905`; want `daimilitary.c:913-915` | Priced as a normal attack. Unreachable era |
| Upgrade Unit — `ai/default` | `daicity.c:381-445` | Upgrades all eligible if gold clears a floor; no opportunity-cost comparison |
| Conquer City Shrink | `daitools.c:926-949` | Fine; the siege choice lives upstream |
| Home City | `daitools.c:755-776` (one call site) | Never used to redistribute upkeep |
| Disband Unit Recover | `daicity.c:454-521`; 3 callers in deepdefence, 2 in daicity | Exit path only. **Cross-ref S-HEDGE-production-liquidity.md — owns this** |
| Hut/Transport/Move dispatch | `daitools.c:1022-1235` | Correct mechanics; nothing to add |

### USED-AND-VALUED

| Capability | Where | Quality |
|---|---|---|
| Found City | daisettler/deepsettler | The main line |
| Trade Route / Help Wonder | daidomestic, daiunit | Known-covered (R2/R2b) |
| Join City | **deep only** (`deepsettler.c:1325-1440`); stock never performs it (upstream bug #31) | Good; banked |
| Upgrade — `ai/deep` | `deepdefence.c:1110-1144` in the melt | Genuinely valued: obsolescence + upkeep gates, prefers upgrade |
| Paradrop | `daiparadrop.c:61-175`, want `:340-374` | Real logic, unreachable era |
| Spy/Diplomat family | `daiactions.c:96-303`, `daidiplomat.c:218-695` | Fires (36 steals measured). But a **hardcoded lexicographic ladder** (10000…2000) under the explicit FIXME "utility values are NOT proportional to other AI utilities" — classic RC4. `SPY_ATTACK`, `STRIKE_*`, `BRIBE_STACK`, `SPY_ESCAPE` absent from the ladder entirely |

**Not a use, do not miscount:** the `ACTRES_*` switches at `daicity.c:1236-1345` (building-vs-action defence want), `daidiplomacy.c:2002-2100` (incident classification), `advdata.c:848-950` (government immunity) are reactive classification, not action use.

---

## 2. Ruleset asymmetries (Rapture_Grow precedent)

1. **`Enter Marketplace` is classic-only.** `ai/default` treats it as the caravan fallback in ~10 places; **on civ2civ3 the entire fallback branch is unreachable.** Direct Rapture_Grow analogue; worth an R1-style audit.
2. **`Conquer Extras` is classic-only** — `daitools.c:999-1021` dead on civ2civ3; the AI cannot take an enemy fort by walking onto it there.
3. **`Capture Units` and `Bombard` are civ2civ3-only** — `daitools.c:835-874`, `daiunit.c:452-460` dead on classic.
4. **Fortress semantics differ sharply.** classic: flat +100% defence, `border_sq=5` (claims territory), `NoStackDeath`. civ2civ3: Fort→Fortress two-stage, no borders, no NoStackDeath, forbidden on rivers. **A fortress is worth substantially more in classic.** Neither ever built.
5. **Airlift eligibility**: civ2civ3 `Airliftable` class flag spans Air/Helicopter/Missile/Merchant/Big Land; classic is Land only.
6. Minor: civ2civ3 Explorer embassy/investigate enablers; classic Investigate-spend + paradrop-onto-Fortress enablers.

---

## 3. Ranked opportunities

**1 — Base building (Fortress/Fort).** The cleanest structural hole: mechanism wired, want provably zero (the only valuation reads tile output; a base changes none). 0 fortresses in 175 cities. Pure RC1/RC3: value that isn't tile yield, possibly outside any work radius. Needs a NEW want channel in `ai/deep` (rule 4 — not advisors). Classic venue pays extra (borders + NoStackDeath).

**2 — Capture Units on civ2civ3, both directions.** Turn-1, primary ruleset, 19 capturer types. Offense: a caravan is ~50 shields + a route; a Migrant is a citizen. Defense: nothing knows own caravans/Migrants are capturable, and deep routes caravans through contested ground routinely. Defensive half likely larger and cheaper. **Instrument first** (capturable adjacencies per game).

**3 — Pillage as economic warfare.** Both rulesets, turn 1, written admission it was left out. 664 roads / 371 irrigations undefended at T162; rampagers go home when there's nothing to kill. Needs a deny-enemy-yield want — same missing machinery family as #1.

**4 — Bombard mispricing (civ2civ3).** Real, structural — and near-zero prize in our cells (Bomber at 57 techs). File; don't schedule. Same verdict for Paradrop/Nuke/W32 air: STATUS's era caveat is now answered empirically — negative.

**5 — Airlift.** Double-broken (braindead Airport want ⇒ no Airport; defensive-only doctrine). Behind Radio. Low.

**6 — Marketplace/Conquer-Extras dead-branch audit.** Cheap, bounded, R1-sequel.

**Explicitly not opportunities:** Expel, Suitcase Nuke, Destroy City, Spread Plague, Surgical Strike, Heal, Wipe, Homeless, Teleport, Steal Maps, Convert — no enabler in either ruleset. The Steal-Maps/Spread-Plague valuation stubs are dead code to note, not fix.

**New upstream-bug candidates:** bombard priced as a kill in `dai_rampage_want`; base extras structurally unvaluable in `adv_calc_extra`; `EFT_AIRLIFT` want making Airports unbuildable.
