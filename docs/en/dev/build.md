# Build from Source

## Supported Platforms

- Windows
- Linux
- MacOS

## Prerequisites

This section introduces the prerequisites for compiling clice.

### Toolchain

- clang >= 19
- c++23 compatible standard library
  - MSVC STL >= 19.44(VS 2022 17.4)
  - GCC libstdc++ >= 14 
  - Clang libc++ >= 20
  
clice uses C++23 as the language standard. Please ensure you have an available clang 19 or above compiler, as well as a standard library compatible with C++23.

> clice can currently only be compiled with clang. In the future, we will improve this to allow compilation with gcc and msvc.

### LLVM Libs

- 20.1.5 <= llvm libs < 21

Due to the complexity of C++ syntax, writing a new parser from scratch is unrealistic. clice calls clang's API to parse C++ source files and obtain AST, which means it needs to link llvm/clang libs. Additionally, since clice uses clang's private headers, these private headers are not available in llvm's binary release, so you cannot directly use the system's llvm package.

If you can find the llvm commit corresponding to your system's llvm package, copy the following three files from that commit:

- `clang/lib/Sema/CoroutineStmtBuilder.h`
- `clang/lib/Sema/TypeLocBuilder.h` 
- `clang/lib/Sema/TreeTransform.h`

Copy them to `LLVM_INSTALL_PATH/include/clang/Sema/`.

Besides this method, there are two other ways to obtain the llvm libs required by clice:

1. Use our precompiled version

```bash
# .github/workflows/cmake.yml

# Linux precompiled binary require glibc 2.35 (build on ubuntu 22.04)
$ mkdir -p ./.llvm
$ curl -L "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/x86_64-linux-gnu-release.tar.xz" | tar -xJ -C ./.llvm

# MacOS precompiled binary require macos15+
$ mkdir -p ./.llvm
$ curl -L "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/arm64-macosx-apple-release.tar.xz" | tar -xJ -C ./.llvm

# Windows precompiled binary only MD runtime support
$ curl -O -L "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/x64-windows-msvc-release.7z"
$ 7z x x64-windows-msvc-release.7z "-o.llvm"
```

> [!IMPORTANT]
>
> For debug versions of llvm libs, we enabled address sanitizer during build, and address sanitizer depends on compiler rt, which is very sensitive to compiler versions. So if using debug versions, please ensure your clang's compiler rt version is **strictly consistent** with what we used during build.
> 
> - Windows currently has no debug build of llvm libs because it doesn't support building clang as a dynamic library. Related progress can be found [here](https://discourse.llvm.org/t/llvm-is-buildable-as-a-windows-dll/87748)
> - Linux uses clang20
> - MacOS uses homebrew llvm@20, definitely don't use apple clang

2. Compile llvm/clang from scratch

This is the most recommended approach, ensuring environment consistency and avoiding crash issues caused by ABI inconsistencies. We provide a script for building the llvm libs required by clice: [build-llvm-libs.py](https://github.com/clice-project/clice/blob/main/scripts/build-llvm-libs.py).

```bash
$ cd llvm-project
$ python3 <clice>/scripts/build-llvm-libs.py debug
```

You can also refer to llvm's official build tutorial [Building LLVM with CMake](https://llvm.org/docs/CMake.html).

## Building

After handling the prerequisites, you can start building clice. We provide two build methods: cmake/xmake.

### CMake

Below are the cmake parameters supported by clice:

- `LLVM_INSTALL_PATH` specifies the installation path of llvm libs
- `CLICE_ENABLE_TEST` whether to build clice's unit tests

For example:

```bash
$ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_INSTALL_PATH="./.llvm" -DCLICE_ENABLE_TEST=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
$ cmake --build build
```

### Xmake

Use the following command to build clice:

```bash
$ xmake f -c --dev=true --mode=debug --toolchain=clang --llvm="./.llvm" --enable_test=true
$ xmake build --all
```

> --llvm is optional. If not specified, xmake will automatically download our precompiled binary

## Run Tests

clice has two forms of tests: unit tests and integration tests.

- Run unit tests:

```bash
$ ./build/bin/unit_tests --test-dir="./tests/data" --resource-dir="<LLVM_INSTALL_PATH>/lib/clang/20"
```

- Run integration tests:

```bash
$ pytest -s --log-cli-level=INFO tests/integration --executable=./build/bin/clice --resource-dir="<LLVM_INSTALL_PATH>/lib/clang/20"
```

> resource-dir is clang's built-in header file folder

Or, if you use xmake as the build system, you can directly run tests through xmake:

```shell
$ xmake test --verbose
$ xmake run unit_tests --verbose
$ xmake test integration_tests/default --verbose
```
