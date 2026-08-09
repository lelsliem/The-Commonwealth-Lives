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
set_version("0.1.0")
set_arch("x64")
set_languages("c++23")
set_encodings("utf-8")

-- Static runtime everywhere: matches the core (MultiThreaded) and the F4SE
-- plugin ecosystem, so no VC runtime DLL is needed in-game. The core's
-- LCE.Core.lib is built with the same setting — mixing would fail to link.
set_runtimes("MT")

add_rules("mode.debug", "mode.releasedbg")

--==============================================================================
--  CommonLibF4 (local pinned clone)
--
--  The typed game API + the F4SE:: namespace plugin contract (the F4SE
--  runtime dll is still required in-game; CommonLibF4 replaces it as the
--  static dependency). Vendored at Depends/commonlibf4, pinned to a build
--  whose RUNTIME_LATEST == 1.11.221 — exactly the project's game runtime.
--==============================================================================

add_subdirs("Depends/commonlibf4")

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

        local cfg = "Debug"
        local mode = config.get("mode")
        if mode == "release" or mode == "releasedbg" then
            cfg = "Release"
        end

        -- Configure once (VS generator: no vcvars environment needed), then
        -- build incrementally. Only the library + headers cross the boundary;
        -- LCE_BULD_*_TESTS/SAMPLES/DOCS stay off.
        if not os.isfile(path.join(buildDir, "CMakeCache.txt")) then
            os.exec(
                'cmake -S "%s" -B "%s" -G "Visual Studio 17 2022" -A x64 '
                    .. "-DLCE_BUILD_TESTS=OFF -DLCE_BUILD_SAMPLES=OFF -DLCE_BUILD_DOCS=OFF",
                corePath,
                buildDir)
        end
        os.exec('cmake --build "%s" --config %s --parallel', buildDir, cfg)

        target:add("includedirs", path.join(corePath, "Include"))
        target:add("linkdirs", path.join(buildDir, "Lib", cfg))
        target:add("links", "LCE.Core")
        if cfg == "Debug" then
            target:add("links", "spdlogd") -- spdlog debug build (core convention)
        else
            target:add("links", "spdlog")
        end
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
    add_rule("lce.core")

    add_files("src/**.cpp")
end)
