set_xmakever("2.9.7")
set_project("clice")

set_allowedplats("windows", "linux", "macosx")
set_allowedmodes("debug", "release", "releasedbg")

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
add_requires("lz4")

add_rules("mode.release", "mode.debug", "mode.releasedbg")
set_languages("c++23")
add_rules("clice_build_config")

target("clice-core")
    set_kind("$(kind)")
    add_files("src/**.cpp|Driver/*.cpp")
    add_includedirs("include", {public = true})

    add_packages("libuv", "toml++", "lz4", {public = true})

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
        on_config(function (target)
            local llvm_dynlib_dir = path.join(target:pkg("llvm"):installdir(), "lib")
            target:add("rpathdirs", llvm_dynlib_dir)
        end)
    elseif is_mode("release", "releasedbg") then 
        add_packages("llvm", {public = true})
        add_ldflags("-Wl,--gc-sections")
    end 

target("clice")
    set_kind("binary")
    add_files("src/Driver/clice.cc")

    add_deps("clice-core")

    on_config(function (target)
        local llvm_dir = target:dep("clice-core"):pkg("llvm"):installdir()
        target:add("installfiles", path.join(llvm_dir, "lib/clang/(**)"), {prefixdir = "lib/clang"})
    end)

    after_build(function (target)
        local res_dir = path.join(target:targetdir(), "lib")
        if not os.exists(res_dir) then
            local llvm_dir = target:dep("clice-core"):pkg("llvm"):installdir()
            os.vcp(path.join(llvm_dir, "lib/clang"), res_dir)
        end
    end)

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
    add_packages("llvm")

    add_tests("default")

    on_test(function (target, opt)
        import("lib.detect.find_tool")

        local uv = assert(find_tool("uv"), "uv not found!")
        local argv = {
            "run", "pytest",
            "--log-cli-level=INFO",
            "-s", "tests/integration",
            "--executable=" .. target:dep("clice"):targetfile(),
            "--resource-dir=" .. path.join(target:pkg("llvm"):installdir(), "lib/clang/20"),
        }
        local opt = {envs = envs, curdir = os.projectdir()}
        os.vrunv(uv.program, argv, opt)

        return true
    end)

rule("clice_build_config")
    on_load(function (target)
        target:add("cxflags", "-fno-rtti", {tools = {"clang", "gcc"}})
        target:add("cxflags", "/GR-", {tools = {"clang_cl", "cl"}})
        -- Fix MSVC Non-standard preprocessor caused error C1189
        -- While compiling Command.cpp, MSVC won't expand Options macro correctly
        -- Output: D:\Desktop\code\clice\build\.packages\l\llvm\20.1.5\cc2aa9f1d09a4b71b6fa3bf0011f6387\include\clang/Driver/Options.inc(3590): error C2365: “clang::driver::options::OPT_”: redefinition; previous definition was 'enumerator'
        target:add("cxflags", "/Zc:preprocessor", {tools = {"cl"}})

        target:set("exceptions", "no-cxx")
        
        if target:is_plat("windows") and not target:toolchain("msvc") then
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
                add_versions("20.1.5", "0548c0e3f613d1ec853d493870e68c7d424d70442d144fb35b99dc65fc682918")
            elseif is_plat("linux") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release-lto.tar.xz")
                add_versions("20.1.5", "37bc9680df5b766de6367c3c690fe8be993e94955341e63fb5ee6a3132080059")
            elseif is_plat("macosx") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-release-lto.tar.xz")
                add_versions("20.1.5", "57a58adcc0a033acd66dbf8ed1f6bcf4f334074149e37bf803fc6bf022d419d2")
            end
        else
            if is_plat("windows") then
                if is_mode("release", "releasedbg") then
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
                    add_versions("20.1.5", "899d15d0678c1099bccb41098355b938d3bb6dd20870763758b70db01b31a709")
                elseif is_mode("release") then
                    add_urls("https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-release.tar.xz")
                    add_versions("20.1.5", "47d89ed747b9946b4677ff902b5889b47d07b5cd92b0daf12db9abc6d284f955")
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

        set_prefixdir("clice")

        add_targets("clice")
        add_installfiles(path.join(os.projectdir(), "docs/clice.toml"))

        on_load(function (package)
            local llvm_dir = package:target("clice"):dep("clice-core"):pkg("llvm"):installdir()
            package:add("installfiles", path.join(llvm_dir, "lib/clang/(**)"), {prefixdir = "lib/clang"})
        end)
end
