## 0.7.5 — 2026-08-12
- **Shove force 8** — the punch now visibly moves the victim (3–5 read as a tip-over in loop testing; your INI was still overriding at 3). The jitter caps at 1.15× so a strong draw never hits the ragdoll 10+ zone. Delete a stale `sim.fight.push` line from your INI to inherit the new default. (ADR-0041)

