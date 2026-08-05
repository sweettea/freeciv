# Session brief — M3: the small-mechanism round

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5, the STATUS tail from D1
onward (D1 lands immediately before this session), and the specific
evidence per item below.

## THE JOB — four small mechanisms, one session

1. **Density-fear reachability gate.** Spec'd at `0112a65112`
   (STATUS 2026-07-31: "Density-fear reachability gate (small, spec'd)"),
   still unbuilt: `deep_density_fear` charges for rival cities it cannot
   be reached BY. Apply the same reachability law `deep_garrison_menace`
   uses (same-continent full, overseas × `menace_overseas_weight`, or the
   spec's own form if it differs — the spec wins; read it first).
   Counter + A/B on isolated-150 (where unreachable neighbours are the
   norm) + cramped guard.
2. **Hut-risk awareness in the explore want.** STATUS 2026-07-31
   mechanism-epoch queue item 4: classic's hut→barbarian lottery is
   priced around by tuning, not by code. Partially landed since
   (`hut_risk_radius`/`hut_risk_discount` exist, gated on the server's
   barbarian settings — deepexplore.c:77,133). Close the remainder the
   queue meant: verify the discount actually covers the lottery (huts
   popped by explorers far from cities still spawn barbarians near
   whatever pops them) — if the existing radius form already covers it,
   say so with evidence and close the item instead of building.
   Venue: huts.conf / classic cells.
3. **Drop-pick cosmetics.** Seed-205 watch item: 633 drop-pick cycling
   events persist cosmetically in the ferry protocol. Diagnose whether
   any real cost exists (turns lost, path thrash) before touching code —
   if cosmetic, the deliverable is the measurement and a WONTFIX note,
   not a change.
4. **Explorer census creep.** Watch item: explorer count crept to 47 on
   seed 205 as newly reached islands reopen explore want. The divisor
   already counts explorers under construction; check whether the fleet
   supply count includes explorers ON OTHER CONTINENTS that cannot serve
   the local unknown (reachability again). Small fix or measured WONTFIX.

5. **Founder-pool reachability (expansion audit §3.8).** Same law as
   item 1, applied to `deep_remaining_sites()` (`deepdefence.c:518`):
   the free-land census is a `whole_map_iterate` with no reachability
   filter, so island cells hold surplus founders against land they can
   never reach, inflating the Join City gate. Reuse item 1's reachability
   test; counter + the founder-pool/join metrics on isolated-150.
6. **Producer tags for R3's corpus (verified 2026-08-05).** The
   `deep-want` calibration line's producer field is `"-"` unless
   `ADV_CHOICE_TRACK` is compiled (`deepwant.c:98-101`) — R3's corpus
   would arrive missing its key column. Make producer tagging available
   in benchmark builds: a meson option (`aitiming` precedent) or an
   always-on cheap tag — smallest behaviour-neutral form wins; prove
   neutrality byte-identically.
7. **The health-credit currency (economy audit §3.11, still unfixed).**
   `deephorizon.c:879-887` pays raw `EFT_HEALTH_PCT` percent points ×
   weight — a third currency beside tile-sums and gold. Convert to the
   tile-sum form the other credits use (the catchment machinery is right
   there), or explicitly re-scope the knob as pile-only with a comment.
   Small; D2 fixed upkeep's dimension and left this one.
8. **Two comments that claim honesty the code lacks (census §5.6).**
   `deepcombat.c:181` ("Real observed units only" — no observation gate
   exists) and `deepdefence.c:736-743` (`score.units` called public —
   the historian broadcasts rank+name only). Make them say what is true.
   Two-line commits; the non-omniscient roadmap lists them as owed now.
9. **Civil-war splinter arm purity — verify and close.** The 2026-07-31
   bug-round item 5 ("splinter players inherit the server-default
   aitype; pin per arm") has no recorded resolution. Check run-game.sh
   and a splinter-containing save (E34.3's seed with 5 stances is a
   ready specimen); fix small or close with evidence.

10. **Delete the phantom switch-penalty dock (economy-effects sweep,
    2026-08-05).** Stock docks build wants by `shield_stock ×
    SHIELD_WEIGHTING/15` (`daicity.c:1909`) to model a production-switch
    penalty that AI players at hard are EXEMPT from
    (`citytools.c:3221`, no `H_PRODCHGPEN` at hard). Deep inherits the
    dock through the domestic advisor. Neutralize it at deep's seam
    (verify the exemption cites first); counter + A/B direction.

## Deferred OUT of M3 by this analysis (recorded so they are not lost)

- **CM completion** (developed-city state, treasury↔CM solvency state,
  stance gates for CM/trade — economy audit §3.6/3.7/3.10): a real
  mechanism session, not a small item; queued separately.
- **Expected-wait site coupling** (expansion §3.1, the W36 surviving
  lead): medium, own session.
- **Air units invisible to the survival wave** (military G5): substrate
  correctness — moved into M1's brief, where the substrate is open.
- **Catchment-scan hoisting** (economy §3.15): behaviour-identical perf;
  fine any time, riding whichever session next touches deephorizon.c
  with a byte-identity proof.

## Discipline

Each item: its own commit(s), its own counter, its own before/after
numbers, on the venue where it has population. Items 3 and 4 are
explicitly allowed to end as "measured, no change warranted" — that is a
result, not a failure (W36 precedent). No invented constants; new knobs
are PLACEHOLDERs registered everywhere + flagged.

## Acceptance

- Per-item verdict table in STATUS: built/closed-with-evidence/WONTFIX,
  with numbers.
- Reachability gate: counter proves it fires on isolated-150; A/B 20
  paired seeds there + cramped guard 10; the gate must NOT weaken cramped
  (where everyone is reachable, it should be a no-op — prove by counter
  ≈ 0 there).
- `meson test` (3) + `tune-audit.py` CLEAN at its count.
- STATUS.md dated section final; leave NEXT-SESSION.md and sessions/
  alone.

## Scope fences

4-7 commits, `ai/deep` only. No floor work (G1 next), no fear-lattice
redesign beyond the reachability gate, no tuning.

## Stop conditions

The 0112a65112 spec contradicts the current fear code (stale plan — report
which half moved); any A/B regresses beyond noise without attribution.
