# Capstone — Decapitation strike (integration test, not an epic)

Full write-up: `findings/decapitation-strike.md`. This is the acceptance test for
whether W31+W32+W33+W35 actually compose. Not a workstream — don't schedule it as
an epic; run it once the pieces exist.

## Why the capital, specifically (confirmed `citytools.c:2022`)
- Capturing a capital with a spaceship STARTED/LAUNCHED → `spaceship_lost()`,
  GUARANTEED destruction. The AI's only current answer to an enemy spaceship is to
  threaten war (`daidiplomacy.c:1495`); it never plans the op that stops it.
- Capturing a capital rolls `civil_war_triggered()`: prob = govt base
  (EFT_CIVIL_WAR_CHANCE) − Σ city bonuses (Courthouse/Police). The AI can compute,
  BEFORE committing, the odds of fracturing the enemy — often a knockout.
Both effects are legible and pre-computable → this is an EV calculation, not a gamble.

## The operation (op_type = DECAPITATION_STRIKE over W33)
1. INTEL (W35+W04) — embassy confirms spaceship state, govt civil-war base, garrison,
   production. Go/no-go on intel confidence + expected value.
2. STAGE (W32/W31) — airbase chain toward the capital, or amphibious task force muster.
3. PREP (W32/W31/W35) — carpet-bomb garrison / naval bombard / spy-sabotage defences.
4. EXECUTE — paradrop (PARADROP_CONQUER) onto the emptied capital, or amphibious land.
   Server fires spaceship_lost() + civil-war roll.
5. HOLD — garrison vs counter-attack, or exploit the fracture.
6. ABORT at any phase if intel stales, garrison reinforces, or spaceship already landed.

## Acceptance (the whole-stack integration test)
- Corpus: enemy one launch from a space win, capital reachable only by air → AI
  builds airbase chain, confirms garrison by espionage, carpet-bombs + paradrops,
  destroys spaceship. Assert full phase sequence.
- Civil-war EV: high-prob target (Despotism) valued above low-prob (Democracy+Courthouses).
- Go/no-go: with garrison unknown, INTEL runs before committing; aborts if unconfirmable.
- Defence symmetry: as target, a deep AI detects the incoming chain and raids it,
  garrisons its capital vs paradrop.
- Abort: spaceship lands before completion → op aborts rather than taking a valueless capital.
