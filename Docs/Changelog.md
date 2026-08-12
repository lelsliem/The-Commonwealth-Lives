## 0.7.5 — 2026-08-12
- **Shove force 8** — the punch now visibly moves the victim (3–5 read as a tip-over in loop testing; your INI was still overriding at 3). The jitter caps at 1.15× so a strong draw never hits the ragdoll 10+ zone. Delete a stale `sim.fight.push` line from your INI to inherit the new default. (ADR-0041)

- **Standing shove** — the punch now plays the game's melee hit-reaction first: the victim staggers back (the animation that plays when a punch lands), then falls. `sim.fight.stagger` (1–4, default 2 = medium) and `sim.fight.pushback` (default 25) tune the flinch; `sim.fight.stagger = 0` = fall only. (ADR-0042)
