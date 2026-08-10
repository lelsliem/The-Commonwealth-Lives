# Tuning — "Open at Eight, Closed at Eight — Unless You Say Otherwise"

**Stone:** adapter 0.5.0 (tuning from the Configuration service)
**Status:** ✅ **IMPLEMENTED 2026-08-10** — the config file is live;
9/9 adapter suites green (TuningTest pins the parser and the settings
fallbacks). In-game verification pending: the `tuning: loaded ...` line
at startup, and a market-hours override actually changing when settlers
stop walking.
**Related:** core 0.5.0 stone 01 (`SimulationTuning::FromConfiguration`),
the world-facts stone (`WorldFacts.h`'s hours constants became the first
tuning keys), ADR-0014 (tuning is an input, never global state).

---

## The file

One text file next to the DLL — `Data\F4SE\Plugins\TheLivingCommonwealth.ini`
(located via the module's own path, so it works regardless of how F4SE
loaded the plugin). Never written by the plugin; it is the modder's knob.

```
; The Living Commonwealth tuning
sim.memory.fade = 0.2        ; how fast memories fade (core)
sim.memory.forget = 0.1      ; forgotten below this weight (core)
sim.drift.rate = 0.05        ; feelings drift toward neutral (core)
sim.goal.urgency = 0.1       ; urgency gained per second (core)
sim.trust.gain = 0.15        ; a fair trade proves reliability (core)
sim.disposition.gain = 0.1   ; aid and company warm feelings (core)
sim.disposition.loss = 0.25  ; wrongs and fights sour them (core)

market.open.hour = 8         ; the adapter's own keys ride along
market.close.hour = 20
```

Format: one `key = value` per line; `;` and `#` comments, blank lines,
CRLF, and surrounding whitespace all survive; a malformed line (no `=`)
is skipped. **Missing, empty, or unparsable values keep the default** —
the core's rule, and the adapter's: a broken line must never break the
world. Unknown keys are ignored, so one file serves both sides.

## The flow

```
TheLivingCommonwealth.ini
    │  (adapter reads the file once, at startup — before any world)
    ▼
Tuning::ParseConfig(text) ──► LCE::Config::Configuration
    │
    ├─► SimulationTuning::FromConfiguration(config)  ──► m_CoreTuning
    │        (the sim.* keys: memory fade, drift, trust, ...)
    │
    └─► Tuning::AdapterSettingsFrom(config)           ──► m_Settings
             (the adapter's own keys: the market hours)
```

`m_CoreTuning` threads into every core call that takes tuning —
`Update` (needs decay, memory fade, goal urgency) and `ReportOutcome`
(trust/disposition gains). `m_Settings` feeds the world-facts gate:
`PushWorldFacts` and the market announce now ask *"is the market closed
given the tuned hours?"* instead of the compile-time constants.

The startup log lines:

```
tuning: no config file (B:\...\Data\F4SE\Plugins\TheLivingCommonwealth.ini expected) — defaults. Create it to change the sim's feel.
tuning: loaded B:\...\Data\F4SE\Plugins\TheLivingCommonwealth.ini — market 08:00–20:00.
```

## The seams

- **Pure vs. edge** — `Tuning.h` is pure (parse + settings read, no game
  types, testable without the game — TuningTest). Only the file IO
  (module path, read) lives in `Adapter::LoadConfiguration` at the edge.
- **One load, before any world** — the constructor loads it once; the
  sim never re-reads the file mid-world (a mid-session edit is picked up
  on the next game start). Tuning is an input, not global state
  (ADR-0014).
- **Defaults are the old behavior** — with no file, everything behaves
  exactly as before; the file only ever relaxes or tightens the sim.
