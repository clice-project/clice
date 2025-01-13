set_xmakever("2.9.7")
set_project("clice")

set_allowedplats("windows", "linux")
set_allowedmodes("debug", "release")

option("enable_test", {default = false})
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

add_requires("toml++", "libuv")
add_requires("llvm")

add_rules("mode.release", "mode.debug")
set_languages("c++23")
add_rules("clice_build_config")

target("clice-core")
    set_kind("$(kind)")
    add_files("src/**.cpp|main.cpp|Server/*.cpp")
    set_pcxxheader("include/Compiler/Clang.h")
    add_includedirs("include", {public = true})
    add_packages("llvm", {public = true})

target("clice")
    set_kind("binary")
    add_files("src/Server/*.cpp", "src/main.cpp")

    add_deps("clice-core")
    add_packages("libuv", "toml++")

target("test")
    set_default(false)
    set_kind("binary")
    add_files("src/Server/*.cpp", "unittests/**.cpp")

    add_deps("clice-core")
    add_packages("gtest", "libuv", "toml++")

    add_tests("default")
    set_runargs("--test-dir=" .. path.absolute("tests"))

rule("clice_build_config")
    on_load(function (target)
        target:set("toolchains", "clang")

        target:add("cxflags", "-fno-rtti", {tools = {"clang", "gcc"}})
        target:add("cxflags", "/GR-", {tools = {"clang_cl", "cl"}})
        target:set("exceptions", "no-cxx")
    end)

package("llvm")
    if is_plat("windows") then
        add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x64-windows-msvc-release.7z")

        add_versions("20.0.0", "a631381bdda707a07afb7b957263a5f7d28f7c9443d478d4cb42ed2c265b2945")
    elseif is_plat("linux") then
        add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release.tar.xz")

        add_versions("20.0.0", "9488dec9c782466a622c5a2dfe0db0a6cb3ff0a98c07d4e0e594bfbbf818909c")
    end

    if is_plat("windows") then
        add_configs("runtimes", {description = "Set compiler runtimes.", default = "MD", readonly = true})
    end

    if is_plat("windows", "mingw") then
        add_syslinks("version", "ntdll")
    end

    on_install(function (package)
        os.mv("bin", package:installdir())
        os.mv("lib", package:installdir())
        os.mv("include", package:installdir())
    end)
