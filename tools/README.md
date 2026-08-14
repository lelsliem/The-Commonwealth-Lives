# The Living Commonwealth — diagnostic tools

Two ways to prove the sim is healthy, one before the game even loads.

## 1. The in-game hello-world (`sim.diag.selfTest`)

The DLL carries a short battery of **pure, game-free checks** — the codec,
the seeded minds, the decay jitter's determinism, the species behaviour
split, the names (generic / role / deterministic), the bond thresholds,
the translator, and the co-save round-trip. It runs **at plugin load,
before the game world and before any save**, and logs one `diag:` line
per check plus a summary.

**Why it exists:** in a heavy load order, a crash during startup is
ambiguous. This answers it in the first second: if the `diag: self-test —
8/8 checks passed` summary lands in the log, the sim's pure logic is
fine and the crash (or hang, or save corruption) is not ours. If a
`diag: [FAIL]` line lands, the sim's own logic is broken — that *is* us.

**How to use it on a test profile:**

1. Open `Data\F4SE\Plugins\TheLivingCommonwealth.ini` (next to the DLL).
2. Set `sim.diag.selfTest = 1`.
3. Launch the game with your test profile's load order.
4. Open `Documents\My Games\Fallout4\F4SE\TheLivingCommonwealth.log`
   and look for the `diag:` lines at the top.

The gate is off (`0`) by default, so normal play stays quiet. It only
ever runs pure logic — no game API is touched — so it is safe at plugin
load, before any world exists.

## 2. The headless harness (`TheLivingCommonwealth.Tests.exe`)

The build also produces a standalone test executable (no game required)
that runs the full suite — 26 suites covering every pure function:

```bash
./Build/windows/x64/debug/TheLivingCommonwealth.Tests.exe
```

It prints `[ RUN  ]` / `[  OK  ]` / `[ FAIL ]` per suite and
`26/26 suites passed.` at the end. This is the full regression net;
the in-game hello-world is its fast, load-order-aware cousin.
