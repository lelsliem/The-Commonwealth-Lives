# Weather Forms — Load Order Reference (Fallout 4)

Verified against the xEdit weather dump on **2026-08-10** — the exact
list that pinned the radstorm gate in `Adapter::IsRadstorm` (see
`Docs/Design/WorldFacts.md`). This is the catalog behind that decision,
kept for the weather work still to come: weather memory events ("it
rained today"), the living-weather roadmap, and any market gate beyond
radstorms.

## Reading the forms

- **`00`-prefixed FormIDs** (e.g. `0002B52A`) are **vanilla** — stable
  across every load order. Safe to hard-code.
- **`002x`-prefixed FormIDs** carry this load order's mod index (`02`).
  They can shift if the load order changes — never pin a `002x` form
  without re-verifying against a fresh dump.
- The `vNNN` tags are xEdit record versions; `v131` is the current
  revision in this load order.
- Three FormIDs lost their names to terminal wrap (`002486A4`,
  `0023AB9C`, `001209AF`) — the IDs are real, the names unknown.
  Re-dump if they ever matter.
- The `VRWorkshopShared_` prefix marks the VR-safety weather family —
  which is exactly why the NoHazard radstorm was excluded below.

## The radstorm family (the pinned decision)

| FormID | Name | Verdict |
|---|---|---|
| `001C3D5E` | CommonwealthGSRadstorm | **PINNED** — the only form the gate matches |
| `0024A3C0` | VRWorkshopShared_CommonwealthGSRadstormNoHazard | excluded — the hazard is removed by design; the gate is the green air, not the sky |
| `00222394` | CommonwealthGSRadstormOld | excluded — editor record, never set at runtime |
| `002392A3` | CommonwealthGSRadstormBackup | excluded — editor record, never set at runtime |

## The full catalog (dump order, FormID descending)

| FormID | Name | Ver |
|---|---|---|
| `0024A3C1` | VRWorkshopShared_CommonwealthGSOvercastNoHazard | v131 |
| `0024A3C0` | VRWorkshopShared_CommonwealthGSRadstormNoHazard | v131 |
| `002486A5` | CommonwealthOvercast_VBFog | v131 |
| `002486A4` | *(name wrapped in the dump)* | — |
| `0023AB9C` | *(name wrapped in the dump)* | — |
| `002392A6` | CommonwealthSanctuaryClearBackup | v129 |
| `002392A5` | CommonwealthMistyBackup | v129 |
| `002392A4` | CommonwealthGSFoggyBackup | v129 |
| `002392A3` | CommonwealthGSRadstormBackup | v129 |
| `002392A2` | CommonwealthFoggyBackup | v129 |
| `002385FD` | CommonwealthClear2 | v129 |
| `002385FB` | CommonwealthDarkSkies2 | v129 |
| `00226448` | CommonwealthDarkSkies3 | v129 |
| `00225922` | CommonwealthSanctuaryClearNoAttach | v128 |
| `0022239A` | CommonwealthRainBackup | v129 |
| `00222394` | CommonwealthGSRadstormOld | v130 |
| `0021A564` | CommonwealthClearTrailer2 | v128 |
| `0021A563` | CommonwealthClearTrailer1 | v128 |
| `00216A98` | CommonwealthClearBackup | v129 |
| `002115D7` | CommonwealthMistyRainyBackup | v129 |
| `00211221` | VideoVaultExit | v128 |
| `0020F46C` | CommonwealthOvercastBackup | v129 |
| `001F61FD` | CGPrewarNukeFXWeather | v128 |
| `001F61A1` | CommonwealthDusty | v129 |
| `001F2529` | CommOvercastTest2 | v126 |
| `001EB2FF` | CommonwealthPolluted | v126 |
| `001E5E60` | CommonwealthDarkSkies | v128 |
| `001D670E` | CommonwealthClearestSkies | v131 |
| `001D1CEC` | FXWthrSunlightOffAtNightBlack | v128 |
| `001CD096` | CommonwealthMistyRainy | v131 |
| `001CC186` | CommonwealthMisty | v131 |
| `001CA7E4` | CommonwealthRain | v131 |
| `001C8556` | CommonwealthOvercast | v131 |
| `001C3D5E` | CommonwealthGSRadstorm | v131 |
| `001C3473` | CommonwealthFoggy | v131 |
| `001BD481` | CommonwealthGSFoggy | v131 |
| `001A6994` | CommonwealthSanctuaryClearNukeFog | v112 |
| `001A65F0` | DefaultInteriorWeather | v130 |
| `001A65E5` | ConcMuseumWeather | v119 |
| `00191647` | FXWthrInvertDayNightGS | v111 |
| `00171636` | FXWthrSunlightOffAtNightGlass | v131 |
| `00171621` | DefaultInteriorWeatherNoLUT | v119 |
| `00141AB4` | FXWthrSunlightWhiteBounce | v131 |
| `0012A18E` | CommonwealthSanctuaryClear | v130 |
| `001256FB` | FXNukeWeather | v120 |
| `001209AF` | *(name wrapped in the dump)* | — |
| `00115C64` | GoodneighborWeatherBase | v120 |
| `0010F781` | TCommonwealthMarshOvercast | v129 |
| `0010E3D4` | EditorCloudPreview | v129 |
| `0010D573` | FXInstituteDayNightCycleKey | v129 |
| `00108640` | FXInstituteDayNightCycle | v129 |
| `000FF98F` | PrewarPlayerHouseInteriorWeather | v120 |
| `000F1033` | CommonwealthGSOvercast | v131 |
| `000DB2A1` | MQ203Weather | v120 |
| `000A6858` | WorldMapWeather | v127 |
| `000A1588` | NeutralWeather | v106 |
| `00096C61` | FXWthrMorningOnly | v120 |
| `00088C57` | FXWthrMoonlightOnly | v120 |
| `000777CF` | FXWthrInvertDayNighWarm | v120 |
| `00076A58` | FXConcord01OffatNight | v129 |
| `00075491` | FXWthrSunlightWhite | v131 |
| `0007548F` | FXWthrSunlight | v128 |
| `000747C8` | FXConcord01 | v128 |
| `0006ED5A` | FXWthrInvertDayNight | v40 |
| `0002B52A` | CommonwealthClear | v131 |
| `00029BB8` | FXWthrSunlightOffAtNight | v128 |
| `000016EC` | IstWeather | v120 |
| `0000116E` | DiamondWeatherPastel | v120 |
| `0000116D` | DiamondWeather | v128 |
| `0000116B` | FXDiamondSunlightBounce | v83 |
| `0000015E` | DefaultWeather | v109 |

## What it's for next

- **Weather memory events** — classify `currentWeather` into a small set
  (clear / rain / fog / misty / radstorm) and push a `{ invalid, ... }`
  world fact, so settlers remember "it rained the day the caravan
  arrived".
- **Living weather** (roadmap) — the game already rotates the whole
  Commonwealth sky; the sim only needs the *fact*, never the form.
- **Market gates beyond radstorms** — the design doc deliberately keeps
  rain out of the market gate; if that ever changes, the live sky
  weathers are the ones at `v131` above.
