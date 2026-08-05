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
