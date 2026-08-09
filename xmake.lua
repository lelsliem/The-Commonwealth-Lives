--==============================================================================
--
--  The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth
--  Engine (LCE).
--
--  An F4SE plugin (GPL) that is a *client* of LCE.Core (MIT). It does not
--  reimplement simulation: it calls CreateEntity / DestroyEntity / Remember /
--  Update and reads intents via GetComponent<Intent>. See
--  C:\LivingCommonwealthEngine\Docs\Architecture\PlatformIntegration.md (the
--  contract) and Docs/AdapterProject.md.
--
--  Build system: xmake — the current CommonLibF4 ecosystem standard. The
--  core stays a CMake project; this build drives it from a rule so `xmake`
--  remains the one command.
--
--==============================================================================

set_xmakever("3.0.0")

set_project("TheLivingCommonwealth")
set_version("0.2.0")
set_plat("windows")
set_arch("x64")
set_languages("c++23")
set_encodings("utf-8")

-- Static runtime everywhere: matches the core (MultiThreaded) and the F4SE
-- plugin ecosystem, so no VC runtime DLL is needed in-game. Mode-aware: the
-- core's Debug libs are /MTd, so a debug adapter must be /MTd too — mixing
-- static runtimes fails to link (LNK2038).
if is_mode("debug") then
    set_runtimes("MTd")
else
    set_runtimes("MT")
end

add_rules("mode.debug", "mode.releasedbg")

--==============================================================================
--  CommonLibF4 (local pinned clone)
--
--  The typed game API + the F4SE:: namespace plugin contract (the F4SE
--  runtime dll is still required in-game; CommonLibF4 replaces it as the
--  static dependency). Vendored at Depends/commonlibf4, pinned to a build
--  whose RUNTIME_LATEST == 1.11.221 — exactly the project's game runtime.
--==============================================================================

includes("Depends/commonlibf4")

--==============================================================================
--  LCE.Core (CMake project, linked statically)
--
--  The core is a separate MIT repo built with CMake. This rule builds it on
--  demand into Build/core (never touching the core repo's own Build/) and
--  links LCE.Core + its spdlog backend. Point at another checkout with the
--  LCE_CORE_PATH environment variable. Delete Build/core if you change the
--  path.
--==============================================================================

rule("lce.core", function()
    on_load(function(target)
        import("core.project.config")

        local corePath = os.getenv("LCE_CORE_PATH") or "C:/LivingCommonwealthEngine"
        local buildDir = path.join(os.projectdir(), "Build", "core")

        -- Pin the core: the adapter builds against 0.4.0+ (the snapshot
        -- API: RegisterSerializer / Capture / Restore / Clear). A stale or
        -- missing checkout fails loudly instead of silently building
        -- against the wrong API.
        local versionHeader = path.join(corePath, "Include", "LCE", "Version", "Version.h")
        local content = os.isfile(versionHeader) and io.readfile(versionHeader) or nil
        local major = content and content:match("MajorValue%s*=%s*(%d+)") or nil
        local minor = content and content:match("MinorValue%s*=%s*(%d+)") or nil
        if not (major and minor) or (tonumber(major) == 0 and tonumber(minor) < 4) then
            raise(
                "LCE core at %s is %s.%s — this adapter requires 0.4.0+ "
                    .. "(the snapshot API). Update the checkout or set LCE_CORE_PATH.",
                corePath,
                major or "?",
                minor or "?")
        end

        local cfg = "Debug"
        local mode = config.get("mode")
        if mode == "release" or mode == "releasedbg" then
            cfg = "Release"
        end

        -- One spdlog in the plugin. The core pins spdlog 1.17 via
        -- FetchContent and compiles it with bundled fmt; the plugin's
        -- commonlib-shared uses spdlog 1.16 built with std::format (xrepo,
        -- std_format=true). Two copies do not mix — different versions and
        -- different format backends produce different symbol names (the
        -- first two links proved it). So the core is forced to build against
        -- the same spdlog 1.16, cloned locally from Depends/spdlog (offline,
        -- pinned), compiled with the same SPDLOG_USE_STD_FORMAT define, and
        -- its own spdlog lib is never linked — LCE.Core's references resolve
        -- against commonlib-shared's copy.
        local spdlogTag = "v1.16.0"
        local spdlogSrc = path.join(buildDir, "spdlog-" .. spdlogTag)
        if not os.isfile(path.join(spdlogSrc, "CMakeLists.txt")) then
            os.exec(
                'git clone -q -b %s --depth 1 "%s" "%s"',
                spdlogTag,
                path.join(os.projectdir(), "Depends", "spdlog"),
                spdlogSrc)
        end

        -- Configure + build (VS generator: no vcvars environment needed;
        -- incremental, so repeats are cheap). Only the library + headers
        -- cross the boundary; LCE_BUILD_*_TESTS/SAMPLES/DOCS stay off.
        os.exec(
            'cmake -S "%s" -B "%s" -G "Visual Studio 17 2022" -A x64 '
                .. '-DFETCHCONTENT_SOURCE_DIR_SPDLOG="%s" '
                .. '-DCMAKE_CXX_FLAGS="/DSPDLOG_USE_STD_FORMAT /EHsc" '
                .. "-DLCE_BUILD_TESTS=OFF -DLCE_BUILD_SAMPLES=OFF -DLCE_BUILD_DOCS=OFF",
            corePath,
            buildDir,
            spdlogSrc)
        os.exec('cmake --build "%s" --config %s --parallel', buildDir, cfg)

        target:add("includedirs", path.join(corePath, "Include"))
        target:add("linkdirs", path.join(buildDir, "Lib", cfg))
        target:add("links", "LCE.Core")
        -- spdlog is intentionally NOT linked here (see the note above):
        -- commonlib-shared's std-format spdlog 1.16 satisfies LCE.Core's
        -- references. One spdlog in the plugin.
    end)
end)

--==============================================================================
--  The plugin
--==============================================================================

target("TheLivingCommonwealth", function()
    set_kind("shared")

    -- The commonlibf4.plugin rule wires the whole plugin contract:
    -- F4SEPlugin_Version data (runtime 1.11.221 via REX address library),
    -- the Windows version resource, and install to Data/F4SE/Plugins.
    add_rules("commonlibf4.plugin", {
        name        = "The Living Commonwealth",
        author      = "LCE Contributors", -- TODO: the author's handle
        contact     = "",
        description = "Fallout 4 adapter for the Living Commonwealth Engine",
    })

    add_deps("commonlibf4")
    add_rules("lce.core")

    add_files("src/**.cpp")
end)

--==============================================================================
--  The adapter's test harness (mirrors the core's: bool-returning suites,
--  no framework). Links LCE.Core only — the pure pieces (translator tables,
--  serializers, seeding) are tested without the game. Builds on every
--  build; run it as `xmake run TheLivingCommonwealth.Tests`.
--==============================================================================

target("TheLivingCommonwealth.Tests", function()
    set_kind("binary")
    add_rules("lce.core")
    add_includedirs("src")
    add_files("src/Translator.cpp", "src/Serialization.cpp", "tests/**.cpp")
end)
