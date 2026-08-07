# Session brief — M4b: the deterrence seat (pre-v4)

Branch `epic/w03-horizon-content`, base `48cfefa9f8` (post CONSEQ+WRACE
and CM-completion merge; 196 knobs, audit CLEAN). Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, this brief, the STATUS tail, and
`findings/amortization-audit-2026-08-07.md` (items 1b, 1c, 2c, 2d are
this session's charter). One seat: everything here re-prices DEFENSIVE
value that stock WAGs and deep inherited. The substrate does the work —
`deepodds`' chained-duel DP and the tile-aware
`get_total_defense_power` seam (`deepcombat.c:174`) already produce
E[attacker losses] and P(hold); this session routes those numbers into
wants that currently carry flats.

Items 1–5 are the session. Items 6–7 are stretch: park them with
evidence rather than rushing them.

1. **Killstack prize in the sortie decision.** `deep_kill_sorties`
   (`deepdefence.c`, the two-threshold go/no-go around 1946–2023) never
   values the TARGET: under killstack rules a won sortie destroys the
   whole enemy stack, so the prize is the summed value of the stack,
   not one kill. Add a prize term (stack shield value ×
   `sortie_prize_gain`, PLACEHOLDER) to the sortie want, symmetric with
   how op pricing values conquests. Counter: sorties taken/declined
   with prize sizes. Expected effect: big stacks parked adjacent get
   punished — the derby storm-wave shape.

2. **The deterrence dividend (human, 2026-08-06: "we aren't valuing
   defensive units highly enough... a musket behind walls is pretty
   dang strong").** A defender's priced value today is only its wave
   survival; its real value includes the losses it IMPOSES and the
   attacks it deters. Price the dividend at the defender-want seam:
   E[attacker losses imposed] (the DP already computes this per duel
   chain) × the observed attack pressure on the city (menace/floor
   facts), gated by `deterrence_gain` (PLACEHOLDER). CONSEQ's
   fight-vs-deny design prices exactly this quantity at the write-off
   seam — same term, consume it, don't reimplement. Guard: the
   dividend must not double-count `defence_worth_mult` (which prices
   the CITY, not the defender's attrition value).

3. **Walls as pop-loss insurance.** Stock prices
   `EFT_UNIT_NO_LOSE_POP` at units×2 (`daieffects.c:456`) — a WAG.
   Server mechanics (verified by CONSEQ): capture costs exactly 1 size
   point, and each successful attack costs another under
   `killcitizen`+`UCF_KILLCITIZEN`. Honest price: P(city is attacked
   over the horizon) × expected attacks × the citizen's stream value
   (the CM counterfactual machinery prices a citizen-turn). This is
   the ADDITIONAL stream on top of walls' Defend_Bonus, which the
   tile-aware defense read already captures in odds. Knob:
   `pop_insurance_gain` (PLACEHOLDER).

4. **Barracks as two streams (audit 1c).** Stock: `EFT_HP_REGEN` =
   5c+num (`daieffects.c:459`). Price both real streams: (a) HP
   throughput — faster healing returns rotation units to the line
   sooner; consume rotation's own melt/return cadence for the
   frequency; (b) the veteran margin — +50% power for the unit's WHOLE
   LIFE × expected combats, a stream stock prices as a point WAG.
   Knobs: `heal_stream_gain`, `veteran_stream_gain` (PLACEHOLDERs).

5. **Unit lifetime upkeep at the build seam (audit 1b).** Buildings
   charge gold upkeep against their horizon credit
   (`upkeep_gold_weight`); units charge NOTHING at build time — a
   Warriors kept 200 turns costs ~20× its build price, and derby-3's
   birth guards are the standing demonstration. Charge upkeep ×
   min(E[lifetime], MORT) at the unit-want seam, symmetric with
   buildings. E[lifetime] = MORT to start; where CONSEQ's obsolescence
   ETA exists, truncate to it. Knob: `unit_upkeep_charge`
   (PLACEHOLDER, 0 = off). This one moves EVERY unit want — A/B it
   alone, not only in the bundle.

6. **(Stretch) The Defend_Bonus sweep (audit 2c).** Coastal Defense,
   SAM, SDI, Great-Wall analogues: one credit family priced by the
   substrate against the threat CLASS the class-gate answers — the
   floor-class facts already classify menace by sea/air/land. Do NOT
   hand-price four buildings; write the sweep once. Era-gated members
   (SAM, SDI): if M4a's era-reachability table has landed, consult it;
   if not, park them with a note — do not invent reachability.

7. **(Stretch) Incite-resistance (audit 2d).** `EFT_INCITE_COST_PCT`
   (Courthouse family) is WAG-priced at `daieffects.c:218`; honest
   price is P(hostile incite attempt) × city worth. M4a is building
   the attacker-side spy currency RIGHT NOW in the bench worktree — if
   its numbers are mergeable when you get here, consume them;
   otherwise park with a pointer. Do not build a second spy model.

## Rules (the usual, non-negotiable)

- Zero changes to `ai/default/`, `ai/baseline/`, `server/advisors/`.
  Stock WAG lines are cited as EVIDENCE, never edited.
- Every knob: `struct tuning`, PLACEHOLDER comment, registered in
  params/blocks ini, `tune-audit.py` CLEAN before and after.
- Every mechanism defaults OFF or neutral; off-switch proof = 10/10
  byte-identical smoke scorelogs against the parent sha.
- Counters on every new decision path (`deep-*` log lines).
- One commit does one thing; ≤400 lines; benchmark numbers pasted.
- A/B on a war venue (contested at minimum) for the bundle, PLUS item
  5 alone. `./benchmark/run.sh --quick` at minimum; paste numbers.

## Execution notes

- Work in the MAIN tree; `deepdefence.c` is released (CONSEQ closed).
  Use port 6600 for any server you run.
- M4a is running in `.worktrees/bench` on `epic/m4a` — do not touch
  the worktree, its build, or its branch. The box has 12 cores and
  M4a benchmarks on it too: keep your benchmark parallelism ≤5, or
  wall-clock triples for both of you (load hit 34 last session).
- `/tmp` was wiped by a host restart: result caches and pins are gone;
  rebuild what you need, and treat analysis outputs as
  non-re-derivable — write tables into the findings doc as you go.
- Long benchmark cells: use the tracked-background loop recipe in
  `findings/cm-completion-2026-08-07.md` (nohup+& inside a tool call
  gets reaped; poll DONE markers).
- Keep a progress log at `doc/deep-ai/sessions/progress-M4b.log` and
  COMMIT it with your closing docs.
- Close with: findings doc (`findings/m4b-deterrence-<date>.md`),
  STATUS section with numbers, final sha in your report.
