# The Living Commonwealth — Requirements

**Version 0.8.12 (beta)** — Fallout 4 1.11.221 (Next-Gen)

## Required

| Requirement | Version | Notes |
|---|---|---|
| Fallout 4 | **1.11.221 (Next-Gen)** | The current Steam/GOG build |
| F4SE | **Next-Gen (0.7.8)** | Script Extender — the plugin won't load without it |
| Address Library for F4SE Plugins | Latest | Required by F4SE plugins on the Next-Gen build |

## Optional

| Mod | Why | Notes |
|---|---|---|
| **MCM (Mod Configuration Menu)** | The in-game settings page (5 pages / 53 controls) | Soft dependency — without it the INI alone rules |
| **Realistic Conversations** (Nexus 26531) | NPC-to-NPC voiced chatter and natural greeting pacing | Its 33 GMST overrides ship as `Realistic Conversations.ini` next to the DLL; the file applies the settings at load. Delete the file to disable |

## Files in this package

```
F4SE/Plugins/TheLivingCommonwealth.dll    the plugin (the sim)
F4SE/Plugins/TheLivingCommonwealth.ini    tuning (created with defaults on first run)
F4SE/Plugins/Realistic Conversations.ini  optional compat tuning (delete to disable)
MCM/Config/TheLivingCommonwealth/config.json   the in-game settings page (MCM optional)
MCM/Settings/TheLivingCommonwealth.ini         the page's shipped defaults
TheLivingCommonwealthAnims.esp            the fight kick animation — ENABLE in your load order
README.md                                 what it does, install, tune, known limits
```

## Install

1. Install the three required mods.
2. Extract the archive into `Fallout 4/Data/` (MO2: install as a mod and enable it).
3. Enable `TheLivingCommonwealthAnims.esp` in your load order.
4. Launch. The INI is created with sane defaults on first run; every number is tunable.

Your existing saves migrate forward — a 0.5/0.6/0.7/0.8-era save loads cleanly.

## Beta notes

- Save often and report what you see — this is the first public beta.
- The mod is in active development; changelog and roadmap live at
  <https://github.com/lelsliem/The-Commonwealth-Lives>.
