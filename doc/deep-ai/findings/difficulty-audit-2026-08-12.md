# Difficulty audit — what a skill level does to `ai/deep` today (2026-08-12)

Session S6, branch `epic/s6-difficulty`, base `9ccbf8426e`. Audit + harness
only; **no `ai/deep` behaviour changed this session**. Every line reference
below was read in this tree, not carried from the plan. Where the tree has
moved since the brief was written, §6 says so.

The question this answers, for the ladder session that follows: *if a player
picks "novice" and deep is seated, what actually gets weaker?*

---

## 1. The stock model, verified

`ai/difficulty.c` is the whole difficulty model, and `set_ai_level_directer()`
(`ai/difficulty.c:39-46`) is its only entry point. It writes four things:

```c
handicaps_set(pplayer, handicap_of_skill_level(level));
pplayer->ai_common.fuzzy         = fuzzy_of_skill_level(level);
pplayer->ai_common.expand        = expansionism_of_skill_level(level);
pplayer->ai_common.science_cost  = science_cost_of_skill_level(level);
pplayer->ai_common.skill_level   = level;
```

| knob | novice / restricted | easy | normal | hard / cheating | source |
|---|---|---|---|---|---|
| handicaps | 14 bits | 12 (novice minus H_DANGER, H_PRODCHGPEN) | 6 (H_RATES, H_TARGETS, H_HUTS, H_DIPLOMAT, H_CEASEFIRE, H_NOBRIBE_WF) | H_RATES only | `ai/difficulty.c:73-130` |
| fuzzy /1000 | 400 | 300 | 0 | 0 | `ai/difficulty.c:143-168` |
| science cost % | 250 | 100 | 100 | 100 | `ai/difficulty.c:177-201` |
| expand % | 10 | 10 | 100 | 100 | `ai/difficulty.c:208-232` |

Two facts about this table drive everything else:

- **`H_MAP` and `H_FOG` are set at exactly one level, `AI_LEVEL_AWAY`**
  (`ai/difficulty.c:60-63`), and `H_TARGETS` stops at `normal`
  (`:114`). At every level a human plays against, *both* AIs see the whole
  map. Difficulty in Freeciv has never been an information ladder; it is a
  *competence* ladder. RC6 in `00-context.md` is the same observation.
- **`expand` has exactly one functional consumer in the entire tree**:
  `ai/default/daidomestic.c:507`. `H_EXPANSION` has exactly one:
  `server/advisors/advdata.c:540-556` → `adv->max_num_cities` →
  `ai/default/daidomestic.c:552`. Everything the expansion handicap does, it
  does through those two lines.

`science_cost` is consumed in `common/research.c:1044-1045,1079` and
`fuzzy` only inside `ai_fuzzy()` (`ai/difficulty.c:339-348`), which callers
must invoke explicitly.

---

## 2. Does deep honour skill levels today? Yes, partially — and by inheritance

**Short answer: deep is level-aware, but almost entirely because it forwards
to `ai/default`, not because deep's own code asks.** Where deep replaced a
stock decision with its own, the handicap usually stopped applying.

Deep's own code reads handicaps at 36 sites, of which **33 are `H_MAP`** for
pathfinder omniscience. The non-`H_MAP` reads are the whole of deep's
first-hand difficulty awareness:

| deep site | reads | what it does |
|---|---|---|
| `ai/deep/deeprates.c:374` | `H_RATES` | maxrate, stock's own formula |
| `ai/deep/deepai.c:512,788` | `H_PRODCHGPEN` | deep *imposes* the handicap on itself |
| `ai/deep/deepai.c:567-587` | `H_MAP/H_FOG/H_TARGETS` | `honest_info` knob, **default 0 = off** (`deeptuning.c:255`) |
| `ai/deep/deepdefence.c:6553`, `deepdiplomacy.c:297,1433` | `H_AWAY` | not-playing mode |
| `ai/deep/deepsettler.c:145`, `deepdefence.c:476`, `deepexplore.c:176` | `H_FOG`/`H_MAP` | vision gates |
| `ai/deep/deepdefence.c:4515,5891,5903` | `ai_fuzzy()` | 3 sites, all inherited from stock's defence code |

Note the direction of two of these: `deep_impose_switch_penalty()`
(`deepai.c:505-524`) and `deep_impose_honest_info()` (`deepai.c:554-589`)
*add* handicaps deep's level never gave it. Deep is not uniformly
under-handicapped — it is **selectively** so, and the selection was never
made on difficulty grounds.

---

## 3. Gap table — per handicap, what deep does

Verdicts: **respected** (deep's path honours it, usually by forwarding);
**bypassed** (deep built a parallel path that never asks); **n.a.** (no deep
path exists).

| knob | stock consumer | deep equivalent | verdict |
|---|---|---|---|
| **H_EXPANSION** (build side) | `advdata.c:540-556` → `daidomestic.c:552` | `deepdefence.c:6607,6610` calls `domestic_advisor_choose_build()` | **respected** |
| **`expand`** (worker want) | `daidomestic.c:507` | same call | **respected** |
| **H_EXPANSION** (colony lift) | — (stock's ferry bid rides `founder_want`, `daidomestic.c:595`) | `deepdefence.c:4957-4984`, a `CT_ATTACKER` bid formed on deep's **military** side | **BYPASSED** |
| **H_EXPANSION** (founder pool) | — | `deepsettler.c:1384-1390`, pool sized by `founder_pool_per_site * deep_remaining_sites()` | **BYPASSED** |
| **H_TARGETS** (ops targeting) | `daiunit.c:621-623` (`map_is_known_and_seen`, V_MAIN) | `deepops.c:2611-2613`, `:3597-3599`, `deepdefence.c:5147` — `map_is_known` only | **BYPASSED** |
| **H_TARGETS** (unclaimed units) | `daiunit.c:621` | `deepops.c:2496` forwards to `dai_manage_unit` | respected |
| **H_TARGETS** (explore census) | `advdata.c:426,446` | `deepai.c:604` forwards `dai_data_phase_begin` | respected |
| **H_HUTS** (hut valuation) | `advdata.c:440`; `autoexplorer.c:257` | `deepexplore.c:146` prices every hut on the map at `explore_hut_value`; only `H_MAP` read (`:176`) | **BYPASSED** (valuation) |
| **H_HUTS** (explorer movement) | `autoexplorer.c:257` | server-side, module independent | respected |
| **H_LIMITEDHUTS** | `unittools.c:3373` | none | respected (server-enforced) |
| **H_DANGER** | `daimilitary.c:740,976` | `deepdefence.c:4294` still calls `assess_danger()` | **respected, then diluted** — deep adds 6 unconditional fear terms (`deepdefence.c:4310-4325`) |
| **H_DANGER** (side effect) | `daimilitary.c:976` forces `danger = 1` | `deepdefence.c:1738`: garrison floor early-returns `if (city_data->danger != 0 ...)` | **latent defect** — see §4.3 |
| **H_DEFENSIVE** | `daieffects.c:502,566` | no deep replacement; forwards `dai_build_adv_adjust` (`deepai.c:793`) | respected |
| **H_NOPLANES** | `daiair.c:597-599` | `deepdefence.c:4771` calls `dai_choose_attacker_air()` | respected |
| **H_DIPLOMAT** (build) | `daidiplomat.c:183` | `deepdefence.c:4378,4661` forward to stock | respected |
| **H_DIPLOMAT** (action) | none in stock either | `deepspy.c:383-462`, no handicap test; `spy_enabled = 0` (`deeptuning.c:211`) | n.a. as shipped; bypassed if enabled |
| **H_NOBRIBE_WF** | `daidiplomat.c:642` | deep leaves bribing to stock (`deepspy.c:371-372`) | respected |
| **H_DIPLOMACY** | `daidiplomacy.c:480,503,916,1296` | `deep_peace_wanted()` (`deepdiplomacy.c:276-310`) checks only `H_AWAY`, and `deep_diplomacy_prepass()` (`:1423`) runs *before* stock's pass | **bypassed in part** — but cuts the same direction as the handicap |
| **H_CEASEFIRE** | `daidiplomacy.c:970` | `deepai.c:1062` forwards `dai_diplomacy_actions` | respected |
| **H_REVOLUTION** | `plrhand.c:592` | `deepai.c:1226` forwards; server-enforced | respected |
| **H_RATES** | `plrhand.c:2065`; `daihand.c:213` | `deeprates.c:374`, same formula | respected |
| **H_PRODCHGPEN** | `citytools.c:3221` | `deepai.c:505-524` — deep *adds* it at every level | respected (over-applied) |
| **`science_cost`** | `common/research.c:1044` | common-side, module independent | respected |
| **`fuzzy`** | `ai_fuzzy()` call sites throughout `ai/default` | 3 sites, `deepdefence.c:4515,5891,5903`, all inherited | **thin** — deep replaced most decisions fuzzy used to blur |
| **H_MAP / H_FOG** | pathfinder `omniscience` throughout | 33 deep sites read it; but no level below AWAY sets it | n.a. at every benchmarked level |

---

## 4. The three findings that matter for S7

### 4.1 H_TARGETS is the one place deep is stronger than its level intends

`deep_op_pick_target()` (`deepops.c:2574`, filter at `:2611-2613`) iterates
`aplayer->cities` and filters on `map_is_known()` — *ever* known. Stock at
novice/easy/normal demands `map_is_known_and_seen(..., V_MAIN)`
(`daiunit.c:621-623`) — *currently visible*. Same for retargeting
(`deepops.c:3597-3599`) and the build-side target menu
(`deepdefence.c:5147`).

So at novice, normal and easy, deep plans operations against cities it saw
once and has not laid eyes on since, using `def_ai_city_data(acity)->worth`
— the enemy's own AI valuation. This is the only handicap where deep's
parallel path makes the AI *better* than the difficulty level promises, and
it is the one a player would notice.

The irony worth recording: **deep already wrote the fix.**
`deep_impose_honest_info()` (`deepai.c:575-577`) sets `H_TARGETS` — and
ships with `honest_info = 0` (`deeptuning.c:255`, `/* 0 = off = omniscient,
as shipped */`). S7 does not need new mechanism here; it needs the bit set
per level.

### 4.2 Two expansion paths never meet the damping

`H_EXPANSION` reaches deep through exactly one door — the
`domestic_advisor_choose_build()` call at `deepdefence.c:6607/6610` — and
deep has since built two producers that do not use that door:

1. **Colony lift** (`deepdefence.c:4957-4984`): a ferry bid formed on deep's
   military side, `amortize(colony_lift_weight * lift_demand, ...)`. At
   novice it competes at full strength against a founder bid that
   `daidomestic.c:552` has just divided by 100. Deep keeps building colony
   ferries while the settlers meant to ride them are suppressed 100×.
2. **The founder pool** (`deepsettler.c:1384-1390`): a settler refuses to
   dissolve while `founders <= founder_pool_per_site * deep_remaining_sites()`
   — an inventory target sized purely by *map emptiness*, never by
   `adv->max_num_cities`. **Deep reads `max_num_cities` nowhere in the
   module.**

Note the asymmetry these create: at novice deep's *production* of settlers
is damped correctly while its *stock* of settlers and its *transport* for
them are not. Expect novice deep to expand less than hard deep, but more
than novice stock, and to waste shields on hulls with no passengers.

Everything else on the expansion side is clean, including the paths that
look suspicious: `settler_rush_weight` (`deepdefence.c:7041-7051`), the
stance multiplier (`:7052-7066`) and `deep_scale_domestic_bids()`
(`:6488-6510`) all multiply `founder_want` **upstream** of the advisor, so
the divisor still lands on top of them.

### 4.3 A handicap meant to make novice *more* fearful disables deep's own insurance

`daimilitary.c:976` forces `city_data->danger = 1` under H_DANGER so that
`grave_danger` is not ignored. Deep's garrison floor
(`deepdefence.c:1738-1740`) early-returns when `city_data->danger != 0`,
using "danger is nonzero" as a proxy for "a real threat exists and the
normal machinery will handle it". Under H_DANGER that proxy is *always*
true, so **novice deep silently loses its minimum-garrison floor** — a
mechanism that only ever fires at hard.

This is not an omniscience gap; it is a mechanism gap in deep, recorded here
per rule 3 of the S6 mandate. It needs no `ai/default` change: the floor's
gate should test the real threat, not the handicapped proxy. S7 owns it.

### Smaller, recorded for completeness

- **Hut valuation** (`deepexplore.c:146`): deep prices every hut on the map
  regardless of H_HUTS, then the server refuses to let its explorers walk to
  the ones it cannot see (`autoexplorer.c:257`) and caps the prize at 25
  gold (`unittools.c:3373`). Not a cheat — a *wasted-production* bug at
  novice and easy: an explorer programme sized by prizes deep is handicapped
  out of collecting.
- **Fuzzy is thin.** Stock's 400/1000 decision flip is spread across
  `ai/default`; deep inherits exactly 3 sites, all in defence. Deep at novice
  is therefore *less erratic* than stock at novice by construction. This is
  the gap the build-choice sampler (§5.1) is meant to fill.
- **Peace-seeking** (`deepdiplomacy.c:276-310`) ignores H_DIPLOMACY, but in
  the direction the handicap wants (more willing to make peace). Cosmetic.

---

## 5. Feasibility read on the two randomizer ideas

Both are **S7 design input; nothing was implemented this session.** Recorded
as: where it hooks, what it costs, what would break.

### 5.1 A randomizer on city choices, scaled by difficulty ease

**Where it hooks.** `deep_city_choose_build()`, `deepdefence.c:6522-6875`.
This is the one seam where every producer's bid for a city-turn is
simultaneously in scope: the military choice in `city_data->choice`, the
domestic choice in `newchoice`, and the improvement maximum already read out
by `deep_bid_log()` (`:6443-6468`). The winner is picked by two
`deep_want_cmp()` calls at `:6650` and `:6679`; a sampler replaces those
comparisons with a draw over the candidate set.

**What it costs.** Low, and the seam already exists — this is the cheapest
of the two by a wide margin. Four constraints are load-bearing:

- **Temperature 0 at hard/cheating must be exact argmax**, or the entire
  benchmark corpus (every number in this project was taken at `hard`) is
  invalidated. The comparison functions are total orders on
  `struct deep_want`, so a zero-temperature softmax that falls back to
  `deep_want_cmp` is byte-identical by construction — but it must be a
  *fallback branch*, not a limit of the arithmetic.
- **Tiers are not samplable.** `deep_want_cmp()` compares `tier` before
  `value` (`DWT_EMERGENCY` at the top). A grave-danger defender losing a
  coin flip is not "weaker play", it is a lost city. Sampling must be
  within-tier.
- **The candidate set is currently 2-3 wide at this seam**, not the full
  build menu: the improvement candidates are collapsed by
  `building_advisor_choose()` (`advbuilding.c:274-310`) into a single
  maximum *before* deep sees them. A sampler that only ever chooses between
  "best building", "best unit" and "settler" is a much blunter instrument
  than "a plausible-but-not-best build". Widening it means sampling inside
  `building_want[]`, which is reachable — deep already iterates that array
  at `deepdefence.c:6448-6457` — but it is a second hook, not the same one.
- **Determinism**: draw from `fc_rand()` (the seeded server PRNG `ai_fuzzy`
  already uses, `ai/difficulty.c:344`) so paired seeds stay paired.

**Verdict: feasible and cheap at the 2-3-candidate seam; a second hook is
needed for it to be legible as "picked a worse building".**

### 5.2 A randomizer on the AI's tuning values, larger for worse AIs

**Where it would hook.** `deep_tuning_get()`, `deeptuning.c:566-572`,
returning `&deep_tuning_active` — **a single file-scope global**, read at
**210 call sites across 16 files**.

**What it costs. This is the expensive one, and the cost is structural, not
incremental.** Per-player jitter means per-player values, and there is
currently no player parameter anywhere in the tuning API. Three routes:

1. *Thread a player through the API* (`deep_tuning_get(pplayer)`): touches
   all 210 call sites, and many are in helpers that do not have a
   `struct player *` in scope. Largest change, cleanest result.
2. *Swap the active struct at the phase boundary* — install player P's
   jittered copy in `dpai_data_phase_begin()` (`deepai.c:594`) and restore
   after. Cheap (tens of lines), and it matches how deep already
   phase-scopes state. **But it is only sound if every read happens inside
   that player's own phase.** Any read from a shared-context callback — the
   `choose_building` vtable hook reached from `advbuilding.c:310`, treaty
   evaluation for another player, anything under `advisors/` — would read
   the wrong player's values. That inventory has to be taken before this
   route can be called safe; it is a small piece of work and it is a
   prerequisite, not a detail.
3. *Jitter only the subset of keys read from player-scoped code.* Smallest
   blast radius, weakest effect.

**The other constraints, from the registry itself:** jitter must be confined
to `TUNE_DOUBLE` keys with a non-degenerate registered range and clamp
*inside* that range (`tuning-params.ini` is the authority; `tune-audit.py`
already checks the C side against it). Mode switches and int gates must be
excluded on principle — jittering a mode flips a *mechanism*, which is a
different AI, not a worse one. Sigma 0 at hard/cheating, or the corpus dies.

**And the ordering argument, which is not about cost:** jitter is noise
around the *right* values. Deep's tunables are still largely placeholders
awaiting the tuning pass owed in `NEXT-SESSION.md` §0. Jitter applied now is
noise around arbitrary points, and it would be indistinguishable from the
miscalibration already present.

**Verdict: feasible, but it is a refactor with a prerequisite (the
read-context inventory), and it should follow the tuning pass, not precede
it. The sampler is the dial to build first.**

---

## 6. Where the plan was stale

The S6 brief (`sessions/S-difficulty-audit.md:33-37`) records, from
2026-08-04: *"deep's own code reads only `H_MAP` (17 sites), `H_FOG` (1),
`H_AWAY` (1), and calls `ai_fuzzy()` at exactly 2 sites."*

Eight days later the tree has moved, in deep's favour: `H_MAP` is now 33
sites, `H_RATES` is honoured (`deeprates.c:374`), `H_PRODCHGPEN` is
self-imposed (`deepai.c:505-524`), and `H_TARGETS` has an implementation
behind `honest_info` (`deepai.c:575-577`) that ships off. `ai_fuzzy()` is at
3 sites, not 2 (`deepdefence.c:4515,5891,5903`). The S5 omniscience epic
(STATUS 2026-08-11) is the cause.

Nothing in `ai/difficulty.c` or in stock's consumers has moved since
`0896b34`; the stock half of the brief is exact.

---

## 7. Harness support landed with this audit

`benchmark/run.sh --skill LEVEL` runs any existing config at another AI
skill level. It derives a config differing only in the `SKILL=` line and
runs that, so the level enters the result key through `confhash` and a
novice cell cannot cache-hit hard results. Without `--skill` the key
computation is untouched byte for byte, and `run-game.sh` is not modified at
all, so every existing result directory keeps its name and its DONE markers.

Both arms of a cell share the level, including both modules of a `mixed_XY`
game: `run-game.sh:113` emits the bare level command *after* all `create`
lines, and the argument-less form rewrites every existing AI player and the
default for later ones (`server/stdinhand.c:2036-2050`). That shared level
is exactly the deep-vs-stock-at-L comparison goal (c) asks for.

Validation and the counters that prove the handicaps reach deep are in the
STATUS entry for this session.
