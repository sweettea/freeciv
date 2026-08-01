# Benchmark harness

**Built in Phase 0 (`doc/deep-ai/workstreams/W00-harness.md`). Human-verified
before Phase 1 begins.**

```
run.sh              --quick (10 paired seeds) | --full (100, resumable)
tune.py             CMA-ES over struct tuning. TRAIN seeds only.
report.py           table, confidence intervals, effect sizes
configs/            the measurement matrix
seeds/train.txt     tuning only
seeds/holdout.txt   reporting only -- must never appear in tune.py
results/            content-addressed: (tuning_hash, config, seed)
corpus/             saved positions for L1 decision assertions
```

## Non-negotiables

1. **Pair every comparison on identical seeds.** ~110 games per arm paired vs
   ~550 unpaired for the same statistical power.
2. **Tune on train, report on holdout.** Never both.
3. **Primary metric is score ratio in separate games**, not head-to-head —
   head-to-head is gameable by exploiting one opponent's specific weaknesses.
4. **Include self-play.** If a change helps against baseline but not in self-play,
   suspect exploitation rather than improvement.
5. **Crossover for mixed games.** Run each seed twice with slot assignment
   swapped, to cancel starting-position effects.

## Human verification gate

Before trusting any downstream number, confirm:

1. Same config run twice → identical results
2. Arms A and B saw the same maps (log and diff a per-game map hash)
3. `grep -r holdout tune.py` finds nothing
4. A deliberately-introduced regression is caught at the stated sample size

Item 4 is the best test of the test.

## Two-machine tuning split (2026-08-01)

The campaign parallelises across two boxes because the two halves need
different regimes, not just more cores. Plan and rationale:
`doc/deep-ai/findings/objective-layer.md`.

**Machine A -- economy, military zeroed.** Deep's military surcharge
is pinned off by a start file (econ-priors.ini = the T76/T106
evidence priors overlaid with the zeroed regime), so only the
civilian blocks move and the shield budget they allocate is stable
across candidates:

    python3 benchmark/tune.py \
      --start benchmark/tuning/econ-priors.ini \
      --blocks ECON --rounds 3 \
      --config benchmark/configs/lategame-80.conf,benchmark/configs/isolated-150.conf,benchmark/configs/primary-120.conf \
      --count 8 --gens 4 --pop 6 --jobs $(nproc) \
      --out benchmark/results/tuning/econ

`garden2b-150` (2 players, islands, many settlers/workers, 150 turns)
and `lategame-80` (6 players, techlevel 50, 8 settlers + 10 workers,
dispersion 3, 80 turns) are truncated copies of the shipped cells --
economy ramps, short horizons, little time for a war to decide the
game.

What each economy cell is FOR (human, 2026-08-01): `lategame` is the
high-opportunity build-out test -- given tech and settlers and room,
does deep actually build improvements AND terrain improvements? Judge
it with `buildmix.py` (improvement census) and `terraform.py`, not
score alone. `isolated` exercises SCIENCE over a long peaceful game.
`primary` (shared continent, FRACTAL) is the warlike regime and
belongs to machine B, not here.

**Machine B -- the alternating search (human design 2026-08-01).**
Each round runs a global re-centring pass over every constant, then
refines the civilian side, then the military side; repeat. The
focused passes inherit the centre the global pass found, and the
global pass then re-reads a world the focused passes changed:

    python3 benchmark/tune.py --start benchmark/tuning/europe-priors.ini \
      --blocks ALL,ECON,MIL --rounds 3 \
      --config benchmark/configs/lategame-80.conf,benchmark/configs/isolated-150.conf,benchmark/configs/primary-120.conf \
      --count 8 --gens 4 --pop 6 --jobs $(nproc) \
      --out benchmark/results/tuning/war

**Both machines run the SAME three cells** (human decision
2026-08-01): `lategame-80` (build-out: tech 50, 8 settlers, 10
workers), `isolated-150` (science, peaceful islands) and
`primary-120` (FRACTAL, 400 tiles/player, shared continent -- the
warlike one). Same venues under different regimes is what makes the
two machines' results mergeable, and it gives block S the tension it
needs.

All three enable **citizen migration with `mgr_distance 6`** -- the
maximum the server allows (`1 + CITY_MAP_MAX_RADIUS`). Citizens walk
toward the better empire, so economic quality shows up in city sizes
and score rather than only in build orders. Neither AI models
migration (that is W17), so it is an environmental force applied
equally to both paired arms.

ALL is 81 dimensions, far more than a small-population ES can
search: treat it as coarse re-centring and give it the largest --pop
the box affords. ECON (42) and MIL (39) do the real work.

**The blend must contain a peaceful cell and a warlike one.** S is the fear -> military-want conversion; tuned only on
warlike maps it learns "always arm", which is the exact pathology
measured on 2026-08-01 (deep fielding 1.57x baseline's army with an
economy 25-30% behind). `primary` supplies the war -- FRACTAL, 400
land tiles per player, startpos ALL, i.e. everyone on one continent
and early contact -- and `isolated` supplies the cost of over-arming.
`contested` may be added for M only; it is a mechanism cell (~38 land
tiles per player, see cell sizing below), not a venue for the split.

(There is no `compact.conf`; the small crowded cell is `cramped` --
3 players, 30x30, 300 turns.)

**Cost warning.** `primary` is 300 turns on ~8000 tiles with
TIMEOUT_S=7200: these are long games, and a pop-6 x gens-4 x 8-seed
pass over it is a cloud-box job (see AWS.md), not a laptop job.

**Blended `--config a,b,c` is not the same as `--mixed`.** The first
averages the paired objective over several configs. The second puts
both arms inside ONE game with seat mirroring, which is the only way
to see defects that never fire in deep-vs-deep worlds. Use blended for
the campaign; use `--mixed` for a confirmation pass.

**Merging the two machines, and the experiment hidden in it.**

    # take block E from machine A, blocks S+M from machine B
    python3 - <<'PY'
    def read(p):
        d = {}
        for line in open(p):
            line = line.split(";", 1)[0].strip()
            if line and not line.startswith(("[", "#")):
                k, _, v = line.partition("=")
                d[k.strip()] = v.strip()
        return d
    econ = read("benchmark/results/tuning/econ/best-tuning.ini")
    war = read("benchmark/results/tuning/war/best-tuning.ini")
    blocks = read("benchmark/tuning-blocks.ini")
    e = [x.strip() for x in blocks["E"].split(",")]
    merged = dict(war)
    merged.update({k: v for k, v in econ.items() if k in e})
    with open("benchmark/tuning/merged.ini", "w") as f:
        f.write("[tuning]\n")
        for k, v in sorted(merged.items()):
            f.write(f"{k} = {v}\n")
    PY

    python3 benchmark/tune.py --evaluate benchmark/tuning/merged.ini \
      --config <machine B's config list> --count 8

If the merged file scores at least as well as machine B's own best,
the blocks composed and the split was sound. If it scores WORSE, the
economy constants were tuned against a budget the war constants then
changed -- the coupling is real, and the objective layer has to land
before more tuning is worth buying. Either way the answer is one
cheap evaluation, and it is the same question `--rounds 2` asks on a
single box via the alternation table.

Final numbers come only from `--report-holdout`, never from a
training run.

## Cell sizing: how much room each regime actually gives (2026-08-01)

Human, on `cramped`: "3 players 30x30 seems insanely small, that's
basically room for 4 cities apiece at 30% landmass??" -- correct, and
`contested` is far smaller still. Land per player at ~30% landmass,
and how many non-overlapping 21-tile catchments that affords:

    cell                 land/player   ~cities   observed cities/player
    cramped   30x30 p3          90       4.3     --
    contested 32x32 p8          38       1.8     4.7 deep / 5.2 base
    lategame  40x40 p6          80       3.8     8.3 deep / 10.2 base
    isolated  56x56 p4         235      11.2     --
    primary  ~89x89 p6         400      19.0     (rarely run: slow)

**`contested` gives under two city catchments per player.** It is a
knife fight, not a civilization game, and the observed ~5 cities per
player only happens because catchments overlap and players conquer
each other. Every binary of the 2026-07-31/08-01 combat arc (B9-B16)
was verified there, which is fine for "does the combat layer work"
and wrong for any claim about being better than stock.

Rules of thumb this implies:

- **Headline claims belong on `primary` (and `isolated`)**, the cells
  the matrix was designed around. `contested` is a mechanism cell.
- **Economy constants want room.** Tuning the civilian block where
  there is space for two cities teaches it to win a knife fight.
- Check a finding on at least one roomy cell before believing it. The
  "gap is economic" diagnosis survives that test -- on `lategame`
  deep has FEWER units than baseline (0.688) and still trails cities
  0.80 / mfg 0.69 / wonders 0.45 -- but it survives by a different
  mechanism than on `contested`, which is exactly why one cell is
  never enough.
