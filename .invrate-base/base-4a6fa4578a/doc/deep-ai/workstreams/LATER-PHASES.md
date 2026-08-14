# Later phases

Workstream files for Phases 2–5 have not been extracted yet. **Extract them from
`../PRD-v3.md` Part VI when you reach them**, one file per workstream, following
the format of the Phase 0/1 files in this directory:

> problem → evidence (with line references) → design → acceptance → commits

Deeper background for each is in `../findings/`. The mapping:

| Workstream | Findings document |
|---|---|
| W01 want unification | `better-ai-plan.md`, PRD-v3 §4.3 |
| W02 site evaluation | `better-ai-plan.md` §1 |
| W03 improvements + co-investment | `better-ai-plan.md` §1b, `coinvestment-deadlock.md` |
| W04 knowledge / honest information | `honest-information.md`, `pathfinding-caches.md` |
| W07 expansion geometry | `expansion-and-allies.md` §1 |
| W08 doctrine + ruleset probe | `doctrine-architecture.md`, `doctrine-and-migration.md` |
| W09 full CM parameters | `where-changes-belong.md` §3 |
| W10 combat model | `readme-audit.md` §2.5, §2.8 |
| W12 empire graph | `better-ai-plan.md` §2, `geography-and-allies.md` §3, `terraform-plans.md` |
| W13 campaigns | `better-ai-plan.md` §3 |
| W14 war ledger | `doctrine-architecture.md` §4 |
| W15 geography + allies | `geography-and-allies.md` |
| W16 diplomatic legibility | `call-to-arms.md` |
| W17 migration | `doctrine-and-migration.md` §3 |
| W18 logistics | `exploration-threading-cpu.md` §4 |
| W19 coalitions | `coalitions.md` |
| W20 threading | `threading-obstacles.md`, `scaling.md` |
| W21 difficulty | `honest-information.md` §6 |
| W22 treaty obligations | `treaty-obligations.md` |
| W23 documentation | `readme-audit.md` |
| W24 conversational goals | `conversational-goals.md`, `W24-ally-requests.md` |
| W25 tuning harness | `eval-tuning-threading.md` §2 |
| W26 planning purity | `removing-mutation.md`, `threading-obstacles.md` |
| W27 scaling | `scaling.md`, `pathfinding-caches.md` |
| W29 transform chains | `terraform-plans.md` |
| W30 extension points | `where-changes-belong.md` §3 |
| W31 naval doctrine | `naval-doctrine.md` |
| W32 air doctrine | `air-doctrine.md`, `air-doctrine-addendum.md` |
| W33 operations layer | `operations-layer.md` |
| W34 victory plan + objective layer | `objective-layer.md`, `decapitation-strike.md` |
| W35 espionage & intelligence | `espionage.md` |
| Capstone: decapitation strike | `decapitation-strike.md` |

W31–W35 and the capstone arrived as the military & intelligence bundle
(imported 2026-07-30; workstream files already extracted, unlike the
rest of this table). **W34 was designed 2026-08-04**
(`W34-victory-plan.md`) and is staged: phase A, the objective layer, is
buildable and measurable now and absorbs the accepted-but-unbuilt
proposal in `findings/objective-layer.md`; phase B, the victory plan
proper, is designed and DEFERRED until W04, because every input it
needs is omniscient today and no victory has ever fired in any cell we
run.

## W12 is no longer unstarted (2026-08-13)

Slice 1 landed on `epic/w12-roads`: `ai/deep/deeptrunk.c` builds the
MST over the player's cities and prices its corridor tiles into the
worker auction through the advisor task-value hook
(`STATUS.md`, "W12 slice 1"). It implements steps 1, 2 and half of 3
of the sketch in `findings/better-ai-plan.md` §2, with two deliberate
departures:

- The "finish what you started" bonus is the sketch's
  `is_last_gap_in_segment ? 3 : 1`, which is an invented constant.
  Here it is `amortize(1, gap)` over the corridor tiles still missing
  -- continuous, monotone in the same direction, and carrying no
  number of its own (rule 7).
- The sketch's step 3 says to iterate plan tiles IN ADDITION to
  city-radius tiles. That half is NOT built: the want reaches only
  the corridor tiles the advisor already enumerates, i.e. those
  inside some city radius. Dispatching a worker into the gap between
  two distant cities is slice 2, and it needs a worker task source
  outside `worker_evaluate_improvements()` rather than a want inside
  it.

Steps 4 (rail on trunk vs spur) and 5 (worker-budget allocator) are
untouched.

## Do not skip ahead

The hard orderings in `../00-context.md` §7 exist because violating them costs
rework. In particular: W01 before W02/W03; W05 before W07; W10 and W11 before
W13; W26 before W20.

For the military bundle: W33 (operations substrate) before the
*coordinated* parts of W31/W32/W35; W04 before W35 and before honest
threat assessment throughout; W12 before the sea-lane graph and
airbase chains; W28/W29 before airbase-chain construction. The
opportunistic fallbacks (convoy discipline, escort pairing, invasion
go/no-go — partially landed 2026-07-30) do NOT wait for W33; they
migrate into phase gates when W33 lands. See the intake assessment in
`../STATUS.md` (2026-07-30) for what is already covered and the
measurability caveats before scheduling W32/W35.
