set_xmakever("2.9.7")
set_project("clice")

set_allowedplats("windows", "linux", "macosx")
set_allowedmodes("debug", "release")

option("enable_test", {default = true})
option("dev", {default = true})
option("release", {default = false})
option("llvm", {default = nil, description = "Specify pre-compiled llvm binary directory."})
option("ci", {default = false})

if has_config("dev") then
    set_policy("compatibility.version", "3.0")
    if is_plat("windows") then
        set_runtimes("MD")
        if is_mode("debug") then
            print("Clice does not support build in debug mode with pre-compiled llvm binary on windows.\n"
                .."See https://github.com/clice-project/clice/issues/42 for more information.")
            os.raise()
        end
    elseif is_mode("debug") and is_plat("linux", "macosx") then
        set_policy("build.sanitizer.address", true)
    end

    if has_config("enable_test") then
        add_requires("boost_ut[2.3.1]")
        -- TODO: fix python fetch on mac (from xmake-repo python fetch)
        if not (has_config("ci") and is_plat("macosx")) then
            add_requires("python >=3.12", {kind = "binary"})
        end
    end
end

local libuv_require = "libuv"

if has_config("release") then
    set_policy("build.optimization.lto", true)
    set_policy("package.cmake_generator.ninja", true)

    if is_plat("windows") then
        set_runtimes("MT")
        -- workaround cmake
        libuv_require = "libuv[toolchains=clang-cl]"
    end

    includes("@builtin/xpack")
end

add_requires(libuv_require, "toml++")
add_requires("llvm", {system = false})

add_rules("mode.release", "mode.debug")
set_languages("c++23")
add_rules("clice_build_config")

target("clice-core")
    set_kind("$(kind)")
    add_files("src/**.cpp|Driver/*.cpp")
    add_includedirs("include", {public = true})

    add_packages("libuv", "toml++", {public = true})

    if is_mode("debug") then 
        add_packages("llvm", {
            public = true, 
            links = {
                "LLVMSupport",
                "LLVMFrontendOpenMP",
                "LLVMOption",
                "clangAST",
                "clangASTMatchers",
                "clangBasic",
                "clangDependencyScanning",
                "clangDriver",
                "clangFormat",
                "clangFrontend",
                "clangIndex",
                "clangLex",
                "clangSema",
                "clangSerialization",
                "clangTooling",
                "clangToolingCore",
                "clangToolingInclusions",
                "clangToolingInclusionsStdlib",
                "clangToolingSyntax",
        }})
    elseif is_mode("release") then 
        add_packages("llvm", {public = true})
        add_ldflags("-Wl,--gc-sections")
    end 

target("clice")
    set_kind("binary")
    add_files("src/Driver/clice.cc")

    add_deps("clice-core")

target("helper")
    set_default(false)
    set_kind("binary")
    add_files("src/Driver/helper.cc")

    add_deps("clice-core")

target("unit_tests")
    set_default(false)
    set_kind("binary")
    add_files("src/Driver/unit_tests.cc", "tests/unit/**.cpp")
    add_includedirs(".", {public = true})

    add_deps("clice-core")
    add_packages("boost_ut")

    add_tests("default")

    after_load(function (target)
        target:set("runargs", 
            "--test-dir=" .. path.absolute("tests/data"),
            "--resource-dir=" .. path.join(target:dep("clice-core"):pkg("llvm"):installdir(), "lib/clang/20")
        )
    end)

target("integration_tests")
    set_default(false)
    set_kind("phony")

    add_deps("clice")
    add_packages("python", "llvm")

    add_tests("default")

    on_test(function (target, opt)
        import("private.action.run.runenvs")

        local envs = opt.runenvs
        if not envs then
            local addenvs, setenvs = runenvs.make(target)
            envs = runenvs.join(addenvs, setenvs)
        end

        local test_argv = {
            "-s", "tests/integration",
            "--executable=" .. target:dep("clice"):targetfile(),
            "--resource-dir=" .. path.join(target:pkg("llvm"):installdir(), "lib/clang/20"),
        }
        local opt = {envs = envs, curdir = os.projectdir()}

        if has_config("ci") and is_plat("macosx") then
            os.vrun("pip install pytest pytest-asyncio pytest-xdist")
            os.vrunv("pytest", test_argv, opt)
        else
            local python
            local installdir = target:pkg("python"):installdir()
            if installdir then
                python = path.join(installdir, "bin/python")
            else
                python = "python3"
            end

            local ok = try { function()
                os.vrunv(python, { "-c", "import pytest" })
                return true
            end }
            if not ok then
                os.vrunv(python, {"-m", "pip", "install", "pytest", "pytest-asyncio", "pytest-xdist"})
            end

            os.vrunv(python, {"-m", "pytest", table.unpack(test_argv)}, opt)
        end

        return true
    end)

rule("clice_build_config")
    on_load(function (target)
        target:add("cxflags", "-fno-rtti", {tools = {"clang", "gcc"}})
        target:add("cxflags", "/GR-", {tools = {"clang_cl", "cl"}})
        target:set("exceptions", "no-cxx")
        if target:is_plat("windows") then
            target:set("toolset", "ar", "llvm-ar")
            if target:toolchain("clang-cl") then
                target:set("toolset", "ld", "lld-link")
                target:set("toolset", "sh", "lld-link")
            else
                target:add("ldflags", "-fuse-ld=lld-link")
            end
        elseif target:is_plat("linux") then
            -- gnu ld need to fix link order
            target:add("ldflags", "-fuse-ld=lld")
        end
    end)

package("llvm")
    if not has_config("ci") then
        set_policy("package.install_locally", true)
    end
    if has_config("llvm") then
        set_sourcedir(get_config("llvm"))
    else
        if has_config("release") then
            if is_plat("windows") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x64-windows-msvc-release-lto.7z")
                add_versions("20.1.5", "499b2e1e37c6dcccbc9d538cea5a222b552d599f54bb523adea8594d7837d02b")
            elseif is_plat("linux") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release-lto.tar.xz")
                add_versions("20.1.5", "5ff442434e9c1fbe67c9c2bd13284ef73590aa984bb74bcdfcec4404e5074b70")
            elseif is_plat("macosx") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-release-lto.tar.xz")
                add_versions("20.1.5", "f1c16076e0841b9e40cf21352d6661c7167bf6a76fa646b0fcba67e05bec2e7c")
            end
        else
            if is_plat("windows") then
                if is_mode("release") then
                    add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x64-windows-msvc-release.7z")
                    add_versions("20.1.5", "499b2e1e37c6dcccbc9d538cea5a222b552d599f54bb523adea8594d7837d02b")
                end
            elseif is_plat("linux") then
                if is_mode("debug") then
                    add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-debug.tar.xz")
                    add_versions("20.1.5", "c04dddbe1d43d006f1ac52db01ab1776b8686fb8d4a1d13f2e07df37ae1ed47e")
                elseif is_mode("release") then
                    add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release.tar.xz")
                    add_versions("20.1.5", "5ff442434e9c1fbe67c9c2bd13284ef73590aa984bb74bcdfcec4404e5074b70")
                end
            elseif is_plat("macosx") then
                if is_mode("debug") then
                    add_urls("https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-debug.tar.xz")
                    add_versions("20.1.5", "743e926a47d702a89b9dbe2f3b905cfde5a06fb2b41035bd3451e8edb5330222")
                elseif is_mode("release") then
                    add_urls("https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-release.tar.xz")
                    add_versions("20.1.5", "16f473e069d5d8225dc5f2cd513ae4a2161127385fd384d2a4737601d83030e7")
                end
            end
        end
    end

    if is_plat("linux", "macosx") then
        if is_mode("debug") then
            add_configs("shared", {description = "Build shared library.", default = true, type = "boolean", readonly = true})
        end
    end

    if is_plat("windows", "mingw") then
        add_syslinks("version", "ntdll")
    end

    on_install(function (package)
        if not package:config("shared") then
            package:add("defines", "CLANG_BUILD_STATIC")
        end

        os.vcp("bin", package:installdir())
        os.vcp("lib", package:installdir())
        os.vcp("include", package:installdir())
    end)

if has_config("release") then
    xpack("clice")
        if is_plat("windows") then
            set_formats("zip")
            set_extension(".7z")
        else
            set_formats("targz")
            set_extension(".tar.xz")
        end

        set_bindir(".")
        set_prefixdir("clice")

        add_targets("clice")
        add_installfiles(path.join(os.projectdir(), "docs/clice.toml"))

        on_load(function (package)
            local llvm_dir = package:target("clice"):dep("clice-core"):pkg("llvm"):installdir()
            package:add("installfiles", path.join(llvm_dir, "lib/clang/(**)"), {prefixdir = "lib/clang"})
        end)
end
