# Session brief — the founder ferry chicken-and-egg

From the user's Europe game (play-0822-2253, STATUS 2026-08-23
entries): Deep_Carthago faced Sicily/Sardinia/Iberia across short
water for 110 turns with founder_berth_route=1 in the loaded ini,
built Ironclads but never a settler transport, and its siteladder
logged boat=0 on every line — boat-borne site evaluation never ran
once, for any deep seat. Suspected circularity: no boat -> no
overseas sites evaluated -> no overseas want -> ferry want never
fires -> no boat. DIAGNOSIS ONLY: no code changes, no new knobs.
If a needed counter does not exist, DESIGN it and report — do not
implement (deepsite.c is owned by a sibling epic right now).

## Method

1. **Code archaeology first**: trace the full chain and cite every
   link — where boat-borne candidates enter the site ladder (what
   makes boat= nonzero), what the berth route (founder_berth_route,
   the BERTH epic's STATUS entry) actually changed, where ferry
   demand originates (scale_ferry, boat_water_gate, boat_gate_at_bid,
   hull_commission_weight, the lift/convoy machinery), and which
   direction each dependency points. Name the cycle's edges
   precisely or refute the cycle.
2. **Corpus**: the user's game.log (deep-siteladder boat=, any
   deep-lift/deep-hull/deep-boat lines) and the T110 save (Carthago
   fleet composition — what ships existed). Also check the OTHER
   deep seats: did anyone ever run boat>0 evaluation? Rus/Anglia
   have coasts too.
3. **One probe run if needed** (existing counters only): e.g. a
   short Europe-like game with a scripted pre-built transport for
   one seat — does boat>0 evaluation appear once a boat exists?
   That single contrast confirms or refutes the cycle direction.
   Ports 7300-7349, jobs <=2, detached benchmark/*.sh launch (repo
   root shape; no cd-prefixed compounds), DONE markers recording
   SUCCESS.
4. **Deliverable**: STATUS entry with the dependency graph, the
   verdict (cycle real / gate elsewhere / want too small), the
   fix design (which link to break, which existing seam, what
   counter the fix's A/B needs), and the ini-only experiment if one
   exists for the user's next game.

Guards: read-only on ai/ (worktree optional; if you build for the
probe, own worktree + the standard meson flags + pinned binary with
sha-compare). Power-loss sweep on inherited dirs. One commit one
thing (docs/scripts only); STATUS with pasted numbers; standard
trailer; stop after the STATUS commit.
