# Session brief — decompose the war-venue deficit before touching it

The pv4 confirm (STATUS 2026-08-22, merge b820d6ac7d) measured the
best-resolved number of the campaign: on tech50war-200 holdout seeds,
pv3 deep is **−134.8/game vs the classic baseline arm, t −6.15,
winning 1 seed of 16**. Every block-level question we have asked is
small next to this. This epic is diagnosis only: no fixes, no new
knobs, no behaviour changes. Its deliverable is the deficit's SHAPE
and a ranked hypothesis list with numbers.

## Start from artefacts already paid for

tech50war-200 scorelogs with full tag streams exist in:
- the pv4 epic's runs (.worktrees/pv4 result dirs, 128 games incl.
  per-arm baselines)
- benchmark/results/decaytech-holdout/{tuned,anchor,pv3}/ + baselines
- the autopsy's round-1 arms (.worktrees/autopsy results) including a
  compiled-defaults arm on this venue
Read README.scorelog for the tag set; you have per-turn streams of
score and its components for both deep and baseline games on the SAME
seeds. Mine these before running anything.

## The decomposition, in order

1. **When**: per-turn deficit trajectory (deep total minus baseline
   total, mean over seeds, with a divergence-onset estimate). Does
   the gap open early (expansion era), at first contact, or late?
2. **What**: component split — which scorelog tags carry the deficit
   (population? techs? units built/lost? land settled?) and in what
   proportions at the divergence point vs at the end.
3. **Who**: per-seat distribution within deep games — is it a
   uniform per-seat lag or do a few seats collapse (killed off,
   crippled by war) while others match baseline? Survival counts.
4. **Same for compiled defaults** (autopsy arm): does the deficit
   have the same shape without pv3's values? The pv4 run showed the
   deficit is not GCM; establish whether it is tuning-independent.
5. **Hypotheses**: from the shapes, rank candidate causes with the
   numbers that implicate them (e.g. "deficit is 70% population
   opening at T60 in seats that lost a city" points at defence;
   "uniform tech lag from T20" points at economy). If the retained
   data cannot separate the top two, specify the ONE probe run that
   would (venue, counters, n) — and run it only if it fits in ~40
   games (ports 7300-7349, jobs <=4).

## Guards

Diagnosis only; scripts under benchmark/ committed; the analysis must
be reproducible from checked-in scripts + retained artefacts. If a
result dir you need was pruned, say which and what it cost the
analysis — do not re-run big batches to recreate it. Savegame coords
are NATIVE vs MAP TILE_XY (NATIVE_TO_MAP_POS). One commit one thing;
STATUS entry with the trajectory table, component split, seat
distribution, hypothesis ranking; stop there.
