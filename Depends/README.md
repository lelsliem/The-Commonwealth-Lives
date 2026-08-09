# Depends — third-party clones

Full git clones kept locally for study and offline builds. **None of this
directory is committed** (see `.gitignore`): these are third-party projects
with their own histories, licenses, and `.git` directories.

The set was trimmed once the build's shape was known (2026-08): only the
two clones the build actually uses remain.

| Directory     | What it is                                          | License                     | Used by                     |
|---------------|-----------------------------------------------------|-----------------------------|-----------------------------|
| `commonlibf4` | Typed game API (C++23, static lib) + plugin rule; **the** static dependency | GPL-3.0 + modding/linking exceptions | the plugin (built via `includes("Depends/commonlibf4")`) |
| `spdlog`      | Logging backend (v1.16.0 tag available locally)     | MIT                         | the core's logging backend — the `lce.core` rule clones this tag locally so the core builds offline, version-aligned with the plugin's spdlog |

**Removed (not needed by this build):** `f4se` (source) — the plugin does
not link it; CommonLibF4 replaces it as the static dependency, and the F4SE
**runtime** (`f4se_1_10_*.dll`) is a download, installed via the mod
manager, not built from source. `common` — only F4SE's own build uses it.
`json` and `DirectXTK` — banked for future stones; fetch when actually
wired (YAGNI).

**Re-fetch when needed** (known upstreams; the author can supply the
correct links if any of these are wrong):

| Repo | Upstream | When it would be needed |
|------|----------|-------------------------|
| `f4se` | https://github.com/ianpatt/f4se | only if we ever build the F4SE runtime/loader ourselves (unlikely — the runtime is a download) |
| `common` | https://github.com/CharmedBaryon/common | together with `f4se` (its build dependency) |
| `json` | https://github.com/nlohmann/json | serialization/config stones (co-save records, world-fact data) |
| `DirectXTK` | https://github.com/Microsoft/DirectXTK | rendering stones (far off) |

## Provenance

- `commonlibf4` — `https://github.com/libxse/commonlibf4`, branch `main`,
  tag `1.0.0` + 963 commits (`a4b283fc`). Its `RUNTIME_LATEST` is
  **1.11.221** — matches the project's game runtime.
- `spdlog` — `https://github.com/gabime/spdlog` (the core pins 1.17 via
  FetchContent; the adapter's core build uses the local `v1.16.0` tag to
  match the plugin's xrepo spdlog).

## Recreating this directory

```bat
git clone --recurse-submodules https://github.com/libxse/commonlibf4
git clone https://github.com/gabime/spdlog
```

## Licensing landmine

CommonLibF4 is **GPL-3.0-or-later** with modding and linking exceptions —
that is why the adapter is GPL and the core is MIT, and why the two never
share a tree. See the core's `Docs/ThirdPartyLibraries.md` and
`Docs/Architecture/PlatformIntegration.md` for the full reasoning.
