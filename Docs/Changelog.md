## 0.7.5 — 2026-08-12
- **Shove force 8** — the punch now visibly moves the victim (3–5 read as a tip-over in loop testing; your INI was still overriding at 3). The jitter caps at 1.15× so a strong draw never hits the ragdoll 10+ zone. Delete a stale `sim.fight.push` line from your INI to inherit the new default. (ADR-0041)

- **Standing shove** — the punch now plays the game's melee hit-reaction first: the victim staggers back (the animation that plays when a punch lands), then falls. `sim.fight.stagger` (1–4, default 2 = medium) and `sim.fight.pushback` (default 25) tune the flinch; `sim.fight.stagger = 0` = fall only. (ADR-0042)
- **Sequenced scuffle** — the punch's flinch and the knock-down no longer land in the same frame (the knock was overriding the stagger before it showed). The fall now lands `sim.fight.fall.delay` (0.9s) after the stagger, and every beat — fall, counter-punch, counter-fall, slink-off — fires on its own schedule. (ADR-0043)
- **Assertion fix** — the sequenced scuffle hit a debug STL "vector iterators incompatible" assertion: the queue pushed new beats while iterating itself. Beats are now staged in a local list and appended after the loop. (ADR-0044)
