set_xmakever("2.9.7")
set_project("clice")

set_allowedplats("windows", "linux")
set_allowedmodes("debug", "release")

option("enable_test", {default = true})
option("dev", {default = true})
option("release", {default = false})

if has_config("dev") then
    set_policy("compatibility.version", "3.0")
    if is_plat("windows") then
        set_runtimes("MD")
        if is_mode("debug") then
            print("Clice does not support build in debug mode with pre-compiled llvm binary on windows.\n"
                .."See https://github.com/clice-project/clice/issues/42 for more information.")
            os.raise()
        end
    elseif is_plat("linux") and is_mode("debug") then
        set_policy("build.sanitizer.address", true)
    end

    if has_config("enable_test") then
        add_requires("gtest[main]")
    end
end

if has_config("release") then
    set_policy("build.optimization.lto", true)
    set_policy("package.cmake_generator.ninja", true)

    if is_plat("windows") then
        set_runtimes("MT")
        -- workaround
        add_requires("libuv-lto[toolchains=clang-cl]", {alias = "libuv"})
        add_requires("llvm", "toml++")
    else
        add_requires("llvm", "libuv", "toml++")
    end

    includes("@builtin/xpack")
else
    add_requires("llvm", "libuv", "toml++")
end

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
    add_includedirs(".", {public = true})

    add_deps("clice-core")
    add_packages("gtest")

    add_tests("default")

    on_config(function (target)
        target:set("runargs", 
            "--test-dir=" .. path.absolute("tests"),
            "--resource-dir=" .. path.join(target:dep("clice-core"):pkg("llvm"):installdir(), "lib/clang/20")
        )
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
    if has_config("release") then
        if is_plat("windows") then
            add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x64-windows-msvc-release-lto.7z")
            add_versions("20.0.0", "c985d60825991eb6c7400be1b9872edf1de929908b12b282829efa52fda4c75c")
        elseif is_plat("linux") then
            add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release-lto.tar.xz")
            add_versions("20.0.0", "adeb46c441265a4e442aea1b9d55f3950bc240aa84e2498b106d8dfd64e123cc")
        end
    else
        if is_plat("windows") then
            if is_mode("release") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x64-windows-msvc-release.7z")
                add_versions("20.0.0", "4ef335845ebb52f8237bda3bcc7246b06085fdf5edc5cc6cf7f3a7c9ef655c09")
            end
        elseif is_plat("linux") then
            if is_mode("debug") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-debug.tar.xz")
                add_versions("20.0.0", "7dc045424a9667f20845dec058d211476b84300ebcfc8c3a3aabf41bff37cfd9")
            elseif is_mode("release") then
                add_urls("https://github.com/clice-project/llvm-binary/releases/download/$(version)/x86_64-linux-gnu-release.tar.xz")
                add_versions("20.0.0", "30ba7357eb40000f1d13d92242f7d87c3ff623e62205a41d10334d605739af89")
            end
        end
    end

    if is_plat("linux") then
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

        os.mv("bin", package:installdir())
        os.mv("lib", package:installdir())
        os.mv("include", package:installdir())
    end)

if has_config("release") then
    -- workaround xmake bug
    package("libuv-lto")
        set_base("libuv")
        on_test(function (package) end)

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
