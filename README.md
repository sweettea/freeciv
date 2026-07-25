# Freeciv Deep AI — starter kit

Drop into a fork of [freeciv](https://github.com/freeciv/freeciv) at commit
`0896b34` (or rebase and re-verify the line references).

```
CLAUDE.md                      Invariants. Read automatically every session.
.claude/skills/next-epic/      /next-epic — picks the next epic and sets its goal
run-phase.sh                   Chain epics, halting at human gates
doc/deep-ai/
  00-context.md                ALWAYS load. Root causes, architecture, phasing.
  01-conventions.md            Commits, test pyramid, benchmarking rules.
  GOALS.md                     /goal conditions, ready to paste.
  STATUS.md                    Progress. The source of truth for "what's next".
  PRD-v3.md                    Full plan. Reference, not session context.
  workstreams/                 One file per workstream. Load ONE per session.
  findings/                    The investigation behind the plan.
benchmark/                     Harness spec; built in Phase 0.
```

## Getting started

1. Fork freeciv, check out `0896b34`, copy this tree in.
2. `claude` in the repo root — it reads `CLAUDE.md` automatically.
3. `/next-epic` to begin, or paste the E00.1 goal from `GOALS.md`.
4. **Verify the Phase 0 gate yourself** (`workstreams/W00-harness.md` §5)
   before trusting any downstream number.

## Autonomy

`/goal` runs one epic to a verifiable finish. `./run-phase.sh` chains epics,
stopping at human gates. There is deliberately **no single goal for the whole
project** — see `GOALS.md` for why, and what to do instead.

## The one thing to get right

`ai/baseline/` is a frozen copy of the stock AI and the measurement control for
the entire project. If it ever changes, every benchmark number becomes
incomparable. It is also what lets you play old-style and new-style AIs in the
same game — which works today via `create <name> <aitype>` and survives
save/load.
