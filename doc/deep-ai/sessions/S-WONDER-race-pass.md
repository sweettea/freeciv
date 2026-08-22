# Session brief — the wonder race-pass hole

Flagged from live Europe play: Deep_Osmanli finished with 15 cities
and 1 wonder. Find the earlier flag in STATUS/findings (search for
deepwonder / race-pass / Osmanli) and read ai/deep/deepwonder.c
before anything else. Counter-evidence to hold in mind: the war-gap
decomposition measured deep AHEAD +13.1 on wonder score at T201 on
tech50war-200 — so the hole may be venue- or personality-specific,
and establishing WHERE it manifests is step one, not assumed.

## Scope

1. **Diagnose**: read the race-pass logic (the machinery that decides
   deep is losing a wonder race and passes). Instrument it log-only:
   wonder candidates priced, race assessments (won/lost/contested,
   with the ETA numbers), passes taken, wonders started, completed,
   lost to a rival finishing first. Cheap venue probe: a handful of
   games on one econ venue and one war venue (existing configs),
   plus — if a derby-7-tuned game shows the symptom where defaults
   do not — one Europe-tuning cell. <=20 games total.
2. **Fix ONLY if the diagnosis shows a defect** (e.g. the race
   assessor passing on races it would win, or double-counting a
   rival's progress): behind a knob, byte-identity at default, with
   the counters as mechanism evidence. If the behaviour turns out
   correct (passing races deep genuinely loses), the deliverable is
   the finding, not a fix — record it and stop.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). Ports 7600-7649, jobs <=2 — the box
carries two live epics and a queued probe; this is the lightest
session and must stay light. Build + identity first; run games only
in small batches with a box-load check between (scorelog mtimes
under .worktrees/employ and .worktrees/soft). Tracked background
dies ~60 min; DONE-marker resume. Tuning inis: never append an
existing key; dedupe; read-back audit. One commit one thing;
meson test all suites; knobcensus/tune-audit CLEAN (unchanged if
no knob ships); STATUS with pasted numbers; stop after the
diagnosis (or the gated fix + its identity proof if a defect is
proven).
