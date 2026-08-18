//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   Every launch, a tiny hello: the sim's pure logic proves itself.           //
//                                                                             //
//=============================================================================//

#pragma once

namespace TLC::Diag
{
    //-------------------------------------------------------------------------
    // Diag — the load-order hello-world (0.8.x tooling). At plugin load,
    // before the game world or any save exists, a short battery of pure,
    // game-free checks runs the sim's own logic — the codec, the seeded
    // minds, the names, the bonds, the co-save round-trip — and logs a
    // PASS/FAIL line per check plus a summary. It is the first thing the
    // DLL does, so a crash in a heavy load order is provably "not us" the
    // moment the summary lands (or provably ours the moment it doesn't).
    //
    // Gated by sim.diag.selfTest = 1 in the INI: normal play stays quiet,
    // a test profile turns the diagnostics on.
    //-------------------------------------------------------------------------

    // Whether the INI asks for the self-test. Reads the same
    // Data\F4SE\Plugins\TheLivingCommonwealth.ini the tuning does (via
    // the module path); a missing file, missing key, or empty value
    // means off.
    [[nodiscard]] bool SelfTestRequested();

    // Applies sim.log.level to the F4SE log (0.8.12). The default
    // logger shows DEBUG lines in every build config, so the hot DEBUG
    // writers — the WalkTo issue line, the walk probes — flood an 8 MB
    // session at 600 minds. "info" (the default, and the release
    // behavior) drops them; "debug" / "trace" restore them for
    // development. Runs at plugin load, before the banner, so the very
    // first line of a session obeys it.
    void ApplyLogLevel();

    // Runs the battery. Pure logic only — no game API touched, so it is
    // safe at plugin load, before any world. Logs one line per check and
    // a summary. Returns true when every check passed.
    [[nodiscard]] bool RunSelfTest();
}
