# Supported Platforms (CI Tested)

- Windows
- Linux

# Dependencies

- C++23-compatible compiler (currently only clang >=19 supported)
- libstdc++ >= 14 (for linux)
- CMake/Xmake
- [tomlplusplus](https://github.com/marzer/tomlplusplus)
- [libuv](https://github.com/libuv/libuv)
- [LLVM](https://github.com/llvm/llvm-project) >= 19.1.7
- [googletest](https://github.com/google/googletest) (optional)

clice requires Clang's private headers for compilation:

- `clang/lib/Sema/CoroutineStmtBuilder.h`
- `clang/lib/Sema/TypeLocBuilder.h` 
- `clang/lib/Sema/TreeTransform.h`

Copy these headers to either:
- `<clice_dir>/include/clang/Sema` 
- `<LLVM_INSTALL_PATH>/include/clang/Sema`

we provide a script to download these headers to `<clice_dir>/include/clang/Sema` (require xmake).
```bash
xmake l scripts/fetch-clang-headers.lua
```

# Building

## CMake

We use CMake `find_package` in CONFIG mode for dependency resolution.

The CMake option `-DCLICE_DEV=ON` will automatically fetch `tomlplusplus` and `libuv` via FetchContent. If you prefer system-provided dependencies, do not enable this option.

LLVM precompiled binaries must be installed at `-DLLVM_INSTALL_PATH=path/to/llvm`.

For users who cannot build LLVM from source, we provide precompiled binaries for Windows & Linux.

```bash
# .github/workflows/cmake.yml

# Linux precompiled binary require glibc 2.31 (build on ubuntu 20.04)
$ mkdir -p ./.llvm
$ curl -L "https://github.com/clice-project/llvm-binary/releases/download/20.0.0/x86_64-linux-gnu-release.tar.xz" | tar -xJ -C ./.llvm
# windows precompiled binary only MD runtime support
$ curl -O -L "https://github.com/clice-project/llvm-binary/releases/download/20.0.0/x64-windows-msvc-release.7z"
$ 7z x x64-windows-msvc-release.7z "-o.llvm"
```

```bash
$ cmake -B build -DCLICE_DEV=ON -DCLICE_ENABLE_TEST=OFF -DLLVM_INSTALL_PATH=.llvm
$ cmake --build build
```

## Xmake

When using Xmake, all dependencies will be automatically handled.

Xmake will find system packages using pkg-config or package managers (e.g., pacman, apt, ...).

If not found, Xmake falls back to downloading dependencies from GitHub, then building and installing them locally.

```bash
$ xmake f -c --dev=y --enable_test=n --toolchain=clang
$ xmake
```

> The `dev` option will build dependencies using Ninja and set the runtime library to MD (Windows only).

# For developer

## cmake

Recommended workflow using CMakePresets (require [ninja](https://github.com/ninja-build/ninja)):
```bash
$ cmake --preset release
$ cmake --build --preset release
$ ./build/bin/unit_tests --test-dir="./tests" --resource-dir="./.llvm/lib/clang/20"
```

If your system does not have the clang-20 toolchain, please modify `cmake/toolchain.cmake` to specify the clang version.

## xmake

```bash
$ xmake f -c --toolchain=clang
$ xmake build --all
$ xmake test
```

If your system does not have the clang-20 toolchain, please pass `--toolchain=clang-19` to specify the clang version.
