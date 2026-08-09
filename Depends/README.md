# Depends — third-party clones

Full git clones the author moved into this tree for study and offline
builds. **None of this directory is committed** (see `.gitignore`): these
are third-party projects with their own histories, licenses, and `.git`
directories.

| Directory     | What it is                                          | License                     | Used by                     |
|---------------|-----------------------------------------------------|-----------------------------|-----------------------------|
| `commonlibf4` | Typed game API (C++23, static lib) + plugin rule; **the** static dependency | GPL-3.0 + modding/linking exceptions | the plugin (build via `add_subdirs`) |
| `f4se`        | The F4SE runtime source (loader + dll)              | BSD-3-Clause                | runtime only — the plugin does not link it; CommonLibF4 replaces it statically |
| `common`      | Shared foundation (REL/address library)             | zlib                        | F4SE's build (not the plugin's) |
| `spdlog`      | Logging backend (v1.17.0)                           | MIT                         | the core's logging backend (the core fetches its own; this copy is redundant) |
| `json`        | nlohmann/json                                       | MIT                         | future serialization stones |
| `DirectXTK`   | DirectX Tool Kit                                    | MIT                         | future rendering stones |

## Provenance

- `commonlibf4` — `https://github.com/libxse/commonlibf4`, branch `main`,
  tag `1.0.0` + 963 commits (`a4b283fc`). Its `RUNTIME_LATEST` is
  **1.11.221** — matches the project's game runtime.
- `f4se` — `https://github.com/ianpatt/f4se`, branch `master`,
  `v0.7.8` + 3 commits (`cb39721`).
- `common` — `https://github.com/CharmedBaryon/common` (used by F4SE).
- `spdlog` — `https://github.com/gabime/spdlog`, `v1.17.0` (the core pins
  this version via FetchContent; the local copy is a duplicate kept for
  offline reference — the build does not use it).
- `json`, `DirectXTK` — banked for later stones, not yet wired.

## Recreating this directory

```bat
git clone --recurse-submodules https://github.com/libxse/commonlibf4
git clone https://github.com/ianpatt/f4se
git clone https://github.com/CharmedBaryon/common
git clone --branch v1.17.0 https://github.com/gabime/spdlog
git clone https://github.com/nlohmann/json
git clone https://github.com/Microsoft/DirectXTK
```

## Licensing landmine

CommonLibF4 is **GPL-3.0-or-later** with modding and linking exceptions —
that is why the adapter is GPL and the core is MIT, and why the two never
share a tree. See the core's `Docs/ThirdPartyLibraries.md` and
`Docs/Architecture/PlatformIntegration.md` for the full reasoning.
