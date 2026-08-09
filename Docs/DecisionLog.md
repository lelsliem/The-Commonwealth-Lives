# The Living Commonwealth — Decision Log

We record why, not just what. Every architectural decision is logged — the
core's convention (ADR-0011 there; the same discipline here). Once accepted,
a decision only changes if a better alternative exists.

---

## 0001 — Build System: xmake

**Accepted · 2026-08**

The adapter builds with **xmake**, not CMake. CommonLibF4 (the current
libxse generation) has no CMake support — it is an xmake project — and its
plugin rule (`commonlibf4.plugin`) generates the entire plugin contract:
`F4SEPlugin_Version` data, the Windows version resource, and the
`Data/F4SE/Plugins` install layout. Fighting that with a hand-rolled CMake
wrapper would be worse on every axis (Law 001: simple things).

The core stays CMake. The adapter does not adopt the core's build system;
it *drives* it — the `lce.core` rule configures and builds the core into
`Build/core` and links `LCE.Core` + its spdlog backend statically. `xmake`
remains the one command.

*Four questions:* simpler — yes (the ecosystem's own tooling); belongs —
yes (it is the ecosystem's standard); needed — yes (CommonLibF4 2.x only
builds this way); helps — yes (the handshake is exactly what this stone
proves).

## 0002 — Dependencies: local clones, pinned by provenance

**Accepted · 2026-08**

Dependencies come from the **local clones in `Depends/`**, not from
FetchContent/xrepo. The clones were moved into this tree deliberately and
are pinned by their history to a build whose `RUNTIME_LATEST` is
**1.11.221 — exactly the project's game runtime**. That match is the whole
point: the plugin declares `CompatibleVersions({ F4SE::RUNTIME_LATEST })`
and must never drift from the game it runs in.

The clones are **gitignored** — they are third-party trees with their own
`.git` directories and GPL provenance, and committing them would poison the
repo. `Depends/README.md` records what each is and how it was obtained.
The one unavoidable network fetch is commonlib-shared's own spdlog package
via xrepo (first build only); documented in the README.

*Alternative considered:* FetchContent/xrepo of CommonLibF4 (the core's own
pattern, and the doc's stated preference) — rejected for now because the
pinned local copy already matches the game runtime, and offline builds work.

## 0003 — Runtime: static CRT everywhere

**Accepted · 2026-08**

Static MSVC runtime everywhere — CommonLibF4, commonlib-shared, and the
plugin all build with the static CRT, matching the core
(`MultiThreaded$<$<CONFIG:Debug>:Debug>`) and the F4SE plugin ecosystem.
Mode-aware: debug builds use `/MTd`, release `/MT` — the core's Debug libs
are `/MTd`, and mixing static runtimes fails to link (LNK2038, discovered
on the first link and fixed here). The core decides the setting; everything
then follows. xrepo packages (spdlog) pick the same runtime automatically.

## 0004 — Logging: through LCE's API; eyes via REX::LOG

**Accepted · 2026-08**

The mod logs through **`LCE::Logging`** (the documented channel — never
spdlog directly; ADR-0030 in the core) **and** `REX::LOG` for the visible
F4SE log file. LCE's logger is console-bound (`stdout_color_mt`) — inside a
game process that is invisible — so `REX::LOG::Info` is the eyes
(`My Games/Fallout4/F4SE/TheLivingCommonwealth.log`) and LCE's APIis the contract. If the core's logger ever gains a file sink, the adapter's calls
do not change.

**One spdlog in the plugin.** The core builds spdlog compiled into
`LCE.Core.lib`, and commonlib-shared links its own spdlog (xrepo) as a
public dependency. The adapter links **only the xrepo copy** and lets
LCE.Core's spdlog references resolve against it — linking both put two
spdlog implementations in the DLL (duplicate symbols, first link failed).
Version skew (core 1.17 vs xrepo 1.16) is API-compatible for what LCE
uses and disappears when the core's logging gets a file sink or a
header-only option; noted so it is revisited, not forgotten.

## 0005 — Target: runtime 1.11.221, address library, next-gen F4SE

**Accepted · 2026-08**

The plugin targets the **next-gen runtime 1.11.221** (the game installed on
this machine; also the vendored CommonLibF4's `RUNTIME_LATEST`). It uses
the **Address Library** (`UsesAddressLibrary(true)`) — required in-game,
standard practice, and what keeps the plugin working across runtime
patches. F4SE itself is a runtime-only dependency: CommonLibF4 replaces it
as the static dependency, exactly as its README states.

## 0007 — Core pinned to 0.4.0+ (the snapshot API)

**Accepted · 2026-08**

The `lce.core` rule verifies the core checkout's `Version.h` and refuses
anything below 0.4.0 — the adapter's co-save stone stands on the snapshot
API (`RegisterSerializer<T>`, `Capture`, `Restore`, `Clear`), which landed
in 0.4.0. A stale checkout fails loudly at configure time with a clear
message rather than silently building against the wrong API.

The snapshot contract, as the core states it: a type with no serializer is
not persisted; the snapshot is a **process-local** exchange (the adapter
owns the durable F4SE co-save record, its type names, and its versioning);
`Restore` requires the same registrations; `Clear` keeps them. The core's
Snapshot suite (14/14 green) proves the round-trip — the farmer still goes
to market after save/load.

## 0008 — Translation stone: the settlers wake up

**Accepted · 2026-08**

On GameLoaded the adapter translates every loaded settler into an entity
inside the core's registry: a `FormRef` (the entity knows its game form),
seeded `Needs` (all satisfied), empty `Memory` and `Relationships`. The
predicate is **WorkshopNPCFaction membership** (`0x000337F3`, verified by
parsing the game's own `Fallout4.esm` — the often-cited
"WorkshopSettlerFaction" does not exist in the base game). The game is
read once and never written — the value↔ActorValue write-through belongs
to the executor stone (ADR-0024).

Serializers are registered once at init for every persisted type (Needs,
Memory, Relationships, Goals, Intent, FormRef), exercising the core's
0.4.0 snapshot substrate for the first time. The adapter's first test
harness (3/3 green, no game required) proves the translator tables, the
seeding, and a full Capture/Restore round-trip through the adapter's own
serializers.

## 0006 — Versioning

**Accepted · 2026-08**

The adapter versions independently, starting at **0.1.0** for this scaffold
stone. The core is 0.3.1 and its 0.4.0 milestone births this project, but
the adapter is its own artifact with its own milestone rhythm: on milestone
completion, bump the version in `xmake.lua`, the README badge, and the
plugin version data (generated from `set_version`), then commit after the
author approves.
