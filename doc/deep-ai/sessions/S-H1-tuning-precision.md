# Session brief — H1: double-precision tuning loads, with a guard

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md` §5
(the new float-quantisation rule is this session's subject), and the E34.3
tail of STATUS.md (`bcf9f675ef`).

## The defect (E34.3, measured)

`deep_tuning_init()`'s `TUNE_DOUBLE` path goes through
`secfile_lookup_float_default`, whose default parameter is `float`
(`utility/registry_ini.h:457`) — so the moment ANY tuning file is loaded,
every double tunable the file does not mention is quantised
(0.9 → 0.8999999761581421). Two otherwise-identity arms differing only in
file-vs-none diverged in 12/20 games. `tuning=none` is not a valid control
for a one-key file.

## THE JOB (small: 2-3 commits)

1. Make tuning-file loads preserve double precision for unmentioned keys
   exactly (e.g. look up presence first and only overwrite when the key
   exists, or a double-typed lookup — pick the cleanest that does NOT
   touch `utility/` if avoidable; if a `utility/` change is genuinely the
   right fix, it must be behaviour-neutral for every other caller and
   proven so).
2. **The guard**: a tuning file containing only a `[tuning]` header (zero
   keys) must produce a byte-identical game to `tuning=none` — 10 paired
   seeds, pinned binary, pasted. Add this as a cheap automated check if
   the harness has a natural home for it (tune-audit.py or run.sh's
   preflight), so it cannot regress silently.
3. Quantify the meaning-change: for each archived best-tuning ini under
   `benchmark/tuning/`, state whether any of its key values change
   interpretation (they were written as decimal text and re-parse
   identically — verify, don't assume; the change is about UNMENTIONED
   keys, so archived files should be unaffected — confirm and say so).
4. Update NEXT-SESSION §5's interim rule to say the fix landed; STATUS
   dated section as final commit.

## Acceptance

- Empty-file-vs-none byte-identity 10/10 pasted (this fails on the
  pre-fix binary — show that too, 1 seed suffices).
- `meson test -C build` passes; `tune-audit.py` CLEAN 116/116/116/116.
- No behaviour change under `tuning=none` (byte-identity vs pre-fix
  binary, 5 smoke seeds).

## Scope fences

No new tunables, no value changes, no `ai/default`. Leave sessions/ alone;
NEXT-SESSION §5 edit is in scope (the rule this session retires).

## Stop conditions

The fix genuinely requires changing `utility/` registry behaviour for
other callers (report the seam and stop).
