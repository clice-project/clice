set_xmakever("2.9.7")
set_project("clice")

set_allowedplats("windows", "linux")
set_allowedmodes("debug", "release")

option("enable_test", {default = true})
option("dev", {default = true})

if has_config("dev") then
    set_policy("compatibility.version", "3.0")
    if is_plat("windows") then
        set_runtimes("MD")
    elseif is_plat("linux") and is_mode("debug") then
        set_policy("build.sanitizer.address", true)
    end

    if has_config("enable_test") then
        add_requires("gtest[main]")
    end
end

add_requires("llvm", "libuv", "toml++")

add_rules("mode.release", "mode.debug")
set_languages("c++23")
add_rules("clice_build_config")

target("clice-core")
    set_kind("$(kind)")
    add_files("src/**.cpp|Driver/*.cpp")
    set_pcxxheader("include/Compiler/Clang.h")
    add_includedirs("include", {public = true})

    add_packages("libuv", {public = true})
    add_packages("toml++", {public = true})
    if is_mode("debug") then 
        add_packages("llvm", {
            public = true, 
            links = {
                "LLVMSupport",
                "LLVMFrontendOpenMP",
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

    on_config(function (target)
        target:add("rpathdirs", path.join(target:dep("clice-core"):pkg("llvm"):installdir(), "lib"))
    end)

target("integration_tests")
    set_default(false)
    set_kind("binary")
    add_files("src/Driver/integration_tests.cc")

    add_deps("clice-core")
    -- TODO
    -- add_tests("integration_tests")

target("unit_tests")
    set_default(false)
    set_kind("binary")
    add_files("src/Driver/unit_tests.cc", "unittests/**.cpp")

    add_deps("clice-core")
    add_packages("gtest")

    add_tests("default")
    
    on_config(function (target)
        target:add("rpathdirs", path.join(target:dep("clice-core"):pkg("llvm"):installdir(), "lib"))
        target:set("runargs", 
            "--test-dir=" .. path.absolute("tests"),
            "--resource-dir=" .. path.join(target:dep("clice-core"):pkg("llvm"):installdir(), "lib/clang/20")
        )
    end)

rule("clice_build_config")
    on_load(function (target)
        target:set("toolchains", "clang")

        target:add("cxflags", "-fno-rtti", {tools = {"clang", "gcc"}})
        target:add("cxflags", "/GR-", {tools = {"clang_cl", "cl"}})
        target:set("exceptions", "no-cxx")
        if target:is_plat("windows") then
            target:add("ldflags", "-fuse-ld=lld-link")
        elseif target:is_plat("linux") then
            -- gnu ld need to fix link order
            target:add("ldflags", "-fuse-ld=lld")
        end
    end)

package("llvm")
    if is_plat("windows") then
        if is_mode("release") then
            add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x64-windows-msvc-release.7z")
            add_versions("20.0.0", "ba3fcd482340b8f892f02b9fc2a233a3ba0e0931fdf4e84a3e2a2ec47283d096")
        else 
            os.raise("Clice does not support build in debug mode with pre-compiled llvm binary on windows.\n"
                      .."See https://github.com/clice-project/clice/issues/42 for more information.")
        end 
    elseif is_plat("linux") then
        if is_mode("debug") then
            add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-debug.tar.xz")
            add_versions("20.0.0", "7dc045424a9667f20845dec058d211476b84300ebcfc8c3a3aabf41bff37cfd9")
        elseif is_mode("release") then
            add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release.tar.xz")
            add_versions("20.0.0", "76de8585494955090c79976d8e71bd719f68fc6dc8d06687284cff19266e4c11")
        end
    end

    if is_plat("windows") then
        add_configs("runtimes", {description = "Set compiler runtimes.", default = "MD", readonly = true})
    elseif is_plat("linux") then
        if is_mode("debug") then
            add_configs("shared", {description = "Build shared library.", default = true, type = "boolean", readonly = true})
        end
    end

    if is_plat("windows", "mingw") then
        add_syslinks("version", "ntdll")
    end

    on_install(function (package)
        os.mv("bin", package:installdir())
        os.mv("lib", package:installdir())
        os.mv("include", package:installdir())
    end)
