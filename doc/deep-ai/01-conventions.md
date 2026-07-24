# Conventions

## Commits

`<area>: <imperative verb> <object>`

Areas: `ai/deep` `ai/default` `ai/baseline` `advisors` `common` `server`
`utility` `build` `test` `doc`

- ≤ ~400 substantive lines
- Builds and passes tests alone
- One line, no "and"

Good:
```
ai/deep: weight catchment tiles by probability the city works them
common: add an optional government override to the requirement context
test: assert three workers complete a transform in a third of the turns
```

Bad:
```
ai/deep: improve site evaluation and fix worker assignment    (two things)
ai/deep: various improvements                                  (unreviewable)
fix stuff                                                      (no area, no object)
```

## Epics

An epic is 5–10 commits ending in a measurable change. Where behaviour changes,
**the flag flips in the last commit of the epic** — so the diff that changes
behaviour is small and obvious.

One epic per session.

## Tests

| Level | What | Cost | Runs on |
|---|---|---|---|
| L0 | Unit tests on pure functions | µs | every commit |
| L1 | Position assertions (load save, one turn, assert a decision) | s | every commit |
| L2 | Short scripted games, 50–80 turns | min | every epic |
| L3 | Full paired batches, 250 turns × 100+ seeds | hrs | every workstream |

**L0 and L1 carry most of the value** — deterministic and fast enough to gate
every commit.

Property tests worth writing:
- monotonicity — more food never lowers a site score
- round-trip — want conversions invert within epsilon
- boundedness — no want is NaN, infinite, or wrongly negative
- ordering — a superset catchment scores ≥ its subset

**Golden want-vector snapshots**: for a fixture position, snapshot every city's
production choice and every unit's task. Any diff must be intentional and
explained in the commit message.

## Build and run

```sh
meson setup build && ninja -C build
ninja -C build
./build/tests/unit_tests
./benchmark/run.sh --quick        # 10 paired seeds, directional
./benchmark/run.sh --full         # 100 paired seeds, resumable
./benchmark/report.py
```

## Benchmarking rules

- **Pair every comparison on identical seeds.** Unpaired needs ~5× the games.
- **Tune on `seeds/train.txt`, report on `seeds/holdout.txt`.** Never both.
- **Report the numbers in the commit body.** "Looks better" is not a result.
- Primary metric is **score ratio in separate games**, not head-to-head.
- Include **self-play** in the suite — if a change helps against baseline but not
  in self-play, suspect you're exploiting baseline's specific weaknesses.

## Logging

Every workstream that changes behaviour adds a field to the per-turn decision
vector log. With this many interacting subsystems, that log is the only way to
answer "why did it do that on turn 140."
