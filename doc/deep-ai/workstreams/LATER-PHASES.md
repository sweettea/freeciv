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

## Do not skip ahead

The hard orderings in `../00-context.md` §7 exist because violating them costs
rework. In particular: W01 before W02/W03; W05 before W07; W10 and W11 before
W13; W26 before W20.
