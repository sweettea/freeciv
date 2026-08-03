Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md — start at "The expansion stall was settlers walking
to islands" and read forward to "Session close". Branch
epic/w03-horizon-content.

DO THIS FIRST, BEFORE BUILDING ANYTHING

**Re-run the head-to-head.** Four behaviour changes landed after the
last one was taken, each individually noise-to-mildly-positive, and
that is exactly how an accumulated regression hides.

    ./benchmark/run.sh --quick --config benchmark/configs/lategame-80.conf \
      --arms mixed_dc,mixed_cd --jobs 8 --out RESULTS --server PINNED

Take it against a pinned control on the same seeds and report the
difference-in-differences, not the raw margin — that is what turned a
t=1.55 raw result into t=2.09 two sessions ago. Report the score margin
BY TURN; the endpoint hid the whole story on this cell once already.

The number to beat: T80 margin **+9.73**, measured on `0f126c33b7`
before the idle-worker fallback, Join City and the founder pool landed.
If it has fallen, bisect those three — they are separate commits
(`df9309d5ad`, `5933c431d0`, `182760afda`) precisely so they can be
tested apart.

THEN, IN ORDER

1. **Tune `founder_pool_per_site`.** The only knowingly-guessed
   constant in this work. Measured calibrations: 0.06 from
   `primary-120`, 0.207 from `lategame-80` — a 3.5x spread with a
   documented reason (one cell's settler pool is expanders, the
   other's is strays). Report PER-CELL values. Do not average the
   spread away; it is evidence about the cells, not noise.

2. **The colony lift as an operation.** `findings/settler-missions.md`
   §8 has the full diagnosis and two dead ends that must not be
   rebuilt. Size it first: ~11 overseas cities per game is the honest
   number, and the "12% delivery" framing was a denominator artefact.
   Home is `deepops.c` — a named boat, a named settler, a named
   beachhead, held against re-tasking, with an abort condition.

WHAT NOT TO REBUILD — THE CODE ALREADY EXISTS

Do not re-derive these from prose. Branch **`shelf/refused-2026-08-03`**
carries both as real commits on top of this branch's tip:

    07c48d84ed  SHELF: stop deep destroying its own boat request
    302f185f58  SHELF: route a ferry past the worker dispatch

    git show 07c48d84ed        # read it
    git cherry-pick 07c48d84ed # take it

- **`drop-boatless` `<= 0` -> `== FERRY_NONE`** (`07c48d84ed`). A
  correct bug fix -- FERRY_WANTED is -1, so the old test dropped the
  mission of a settler that had just asked for a boat, and the drop
  cleared the flag. Measured ALONE it is score -1.50, because the churn
  it removes was a crude escape hatch. **It is a prerequisite for the
  lift epic and should land WITH it**, when the queue it makes honest
  is finally served.
- **Ferry dispatch routing** (`302f185f58`). Measured: Transport turns
  carrying 488 -> 196, delivery 12% -> 11%. Goes backwards, because
  stock's ferry manager parks an empty boat rather than fetching
  anyone. Kept as the reference workaround for upstream #33 if the
  dispatch is ever reordered, or if a deep-side lift replaces stock's
  manager.
- A founder pool sized per city: superseded, and in history at
  `5d422d0b44`. Worse than joining freely on every count
  (`unitslost` +1.50, t=+2.30).
- Site scoring to change city spacing. W02b falsified it and the
  negative control this session confirmed it: median founded city sits
  on `citymindist`, ~half are on the floor, and nothing the AI decides
  moves it.

ON `lategame-80` — HUMAN CALL 2026-08-03: KEEP IT

It is an island, tech-saturated regime no other cell tests: 60% of its
settler targets are overseas, `techlevel 50` probes whether tech is
still worth buying, and it surfaced all three of this session's bugs.
Earlier notes in STATUS questioning it were about whether to TUNE
against it, not whether to keep it — and the answer there is to report
per-cell tuned values and treat sharp disagreement as overfit evidence.

BENCHMARK DISCIPLINE — every one of these cost real time

- PIN the binary (freeciv-server + libfreeciv.so + tuning file to a
  scratch dir; RUNPATH is $ORIGIN) and pass `--server`. New pin
  directory per binary; never overwrite one a batch is using.
- /tmp is tmpfs here and IS wiped between sessions. Anything worth
  keeping goes in a commit, not the scratchpad.
- Long batches must use the harness's background mechanism; a
  `nohup ... &` inside a tool call is reaped and dies silently mid-run.
- NEVER edit `deep-best.ini` or a config while a batch is in flight.
- Filter non-benchmark players — barbarians make some games write 61 or
  64 rows for 60 seats.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (94/94/94/94 now).

METHOD NOTES THAT PAID OFF, AND ONE THAT DIDN'T

- Instrument before fixing, and build the diagnostic as a LADDER that
  relaxes every candidate constraint, not the two you suspect. The
  cause of the settler stall was a rung nobody had thought to
  enumerate.
- Check your denominators. "12% of overseas settlers arrive" survived
  two rounds of analysis before it turned out two thirds of the
  denominator was the same settlers re-picking.
- A pre-registration can pass its own stated criterion and still be
  wrong about the mechanism. D2 did exactly that.

STATE

- deep-best.ini: the site block is NOT TUNED. `site_stream_weight` is
  at placeholder 1.0, `site_travel_discount` is the human-set 0.9,
  `founder_pool_per_site` is placeholder 0.06.
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 — DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.
