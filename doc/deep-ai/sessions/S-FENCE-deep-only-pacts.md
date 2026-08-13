# Session brief — FENCE: no peace-or-more with foreign AIs

Human ask (2026-08-13): "can we invent a knob to disallow cross-ai
peace-or-more? I feel deep's alliance abilities should only really
benefit its fellow deeps." Scheduled Next-ish by the human.

This composes with the alliance doctrine already on the books (§11.4
refinement, 2026-08-12): ally-directed surplus is a FEATURE — gifts,
shared vision, coordinated stances are deep's cooperative dividend.
The fence points that dividend at family: deep still signs cease-fires
with anyone, but peace treaties and alliances (and what rides on them)
are reserved for players running the deep AI.

## Mechanism

One gate in ai/deep (deepdiplomacy.c owns the clinch paths):

- Knob `diplo_deep_only_pacts` (int, default 0 = off, byte-identity).
- When 1: for a treaty partner who is an AI and NOT running ai/deep,
  deep neither PROPOSES nor ACCEPTS any clinch at peace or above
  (peace treaty, alliance — and refuse shared-vision/embassy clauses
  bundled with them). Cease-fire and armistice-below-peace remain
  available. Human partners are exempt: the fence is cross-AI only
  (`is_ai(aplayer)` and ai type != deep).
- Existing treaties are not torn up when the knob turns on mid-game;
  the gate covers new agreements and renewals only. (Cheap, and the
  A/B games start at T0 anyway.)
- Detecting "fellow deep": compare the partner's ai_type to our own —
  already how the module identifies itself; no new state.

## Counters (every new path)

- `deep-fence` line per refusal: turn, partner, clinch level refused,
  whether we would have accepted without the fence. The last field is
  the honesty check: a fence that never intercepts a would-be pact is
  inert, and the A/B is then measuring noise.

## Guards

- Byte-identity 10/10 at default 0, including one mixed cell.
- §11.4 mixed-arm-mandatory applies and the mixed venue is the POINT:
  the fence only expresses in games where deep and stock coexist.
  Venue: tech50war-200 mixed, >=16 mirrored seeds. Success = deep
  side score/cities improve (this knob is openly competitive, not
  wash-seeking — it reserves the cooperative dividend for the deep
  team; the wash-or-better fold bar does not apply unless a fold is
  later proposed).
- Watch the war-exposure side effect: refusing peace means longer
  wars. Report wars-active and cities-lost alongside score — a fence
  that wins score by bleeding a frontier seat is a real result but
  must be visible.
- Stock's reaction is not ours to shape (rule 2): stock AIs may keep
  proposing; deep just declines. No ai/default changes.

## Fit

- PEACE (#31) built the cease-fire-trap pricing; the fence must not
  regress it (cease-fire stays allowed precisely so the trap pricing
  still has its instrument).
- GIFT class values (pv3): gift_ally_gain flows only to allies, so the
  fence narrows gift flow to deep allies automatically — no gift-side
  change needed.
- W34 stance machinery reads treaties; refused pacts mean stance vs
  foreign AIs stays hostile-or-neutral, which is the intent.

Usual rules: struct tuning + params/blocks registration, tune-audit
CLEAN, knobcensus entry with parseable predicate, one commit one
thing, off-switch proof, A/B with pasted numbers, STATUS update.
