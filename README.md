<!-- Begin section: Overview -->

# clice

![C++ Standard](https://img.shields.io/badge/C++-23-blue.svg)
[![GitHub license](https://img.shields.io/github/license/clice-project/clice)](https://github.com/clice-project/clice/blob/main/LICENSE)
[![Actions status](https://github.com/clice-project/clice/workflows/CI/badge.svg)](https://github.com/clice-project/clice/actions)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/clice-project/clice)
[![Discord](https://img.shields.io/badge/Discord-%235865F2.svg?logo=discord&logoColor=white)](https://discord.gg/PA3UxW2VA3)


clice is a next-generation language server for modern C++. Leveraging advanced asynchronous task scheduling and intelligent caching, it achieves lower memory usage and faster response times.

Beyond performance, clice offers instantiation-aware template processing, supports switching header contexts between different source files (including non-self-contained headers), and provides comprehensive support for C++20 modules—from code completion to go-to-definition. Our goal is to deliver a truly fast, accurate, and intelligent development companion for C++ developers.

To learn more, visit the [Official Website](https://clice.io) or our [GitHub](https://github.com/clice-project/clice).
You are also welcome to join our [Discord community](https://discord.gg/PA3UxW2VA3).

> [!IMPORTANT]
> Support for header contexts and C++20 modules are core features currently under active development. These will be progressively refined in upcoming releases. Stay tuned!


# Getting Started

## Installation

### With Visual Studio Code Extension

1. Download the latest clice binary from the [Releases Page](https://github.com/clice-project/clice/releases/latest) and install the [VSCode Extension](https://marketplace.visualstudio.com/items?itemName=ykiko.clice-vscode).
2. Add the following configuration to your `.vscode/settings.json` file:
   ```jsonc
   {
    // Optional: Set to an empty string to disable clangd.
    "clangd.path": "",

    // Required: Set this to the path of the clice binary you downloaded.
    "clice.executable": "/path/to/your/clice/executable",
   }
   ```

> [!NOTE]
> This is an early version. Please do not use it in a production environment. Crashes may occur, and we welcome you to submit issues.

***

## Build and Test

### XMake

#### Basic Build

```shell
xmake f -c --dev=true --mode=debug
xmake build --all
```

#### Build with a Specific Toolchain

```shell
xmake f -c --dev=true --toolchain=clang
xmake build --all
```

#### Build with Tests Enabled

```shell
xmake f -c --dev=true --enable_test=true
xmake build --all
```

#### Build with a Custom LLVM Library

```shell
xmake f -c --dev=true --llvm="./llvm"
xmake build --all
```


You can change "./llvm" to your own LLVM library path as needed.

> [!NOTE]
> clice links against a custom LLVM library, which can be downloaded [here](https://github.com/clice-project/llvm-binary/releases/latest).

#### Run Unit Tests

```shell
xmake run --verbose unit_tests
```

#### Run Integration Tests

We recommend using [uv](https://github.com/astral-sh/uv) to manage Python dependencies and versions. After installing uv, run:

```shell
xmake test --verbose
xmake test --verbose integration_tests/default
```

If you prefer not to use uv, you can run the following commands:

```shell
pip install pytest pytest-asyncio
xmake f -m debug && xmake build unit_tests

# The environment variable LLVM_INSTALL_DIR may vary between commits and platforms,
# depending on the value of `set_versions` in the `package("llvm")` section of xmake.lua
LLVM_INSTALL_DIR=./build/.packages/l/llvm/20.1.5/0181167384bb4acb9e781210294c358d/lib/clang/20/ \
  pytest -s --log-cli-level=INFO tests/integration \
    --executable=./build/linux/x86_64/debug/clice \
    --resource-dir=$LLVM_INSTALL_DIR
```

***

### CMake

#### Basic Build

```shell
mkdir build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

#### Build with a Specific Toolchain

```shell
mkdir build
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

#### Build with Tests Enabled

```shell
mkdir build
cmake -B build -G Ninja -DCLICE_ENABLE_TEST=ON
cmake --build build
```

#### Build with a Custom LLVM Library

```shell
mkdir build
cmake -B build -G Ninja -DLLVM_INSTALL_PATH="./.llvm"
cmake --build build
```

You can change "./llvm" to your own LLVM library path as needed.

> [!NOTE]
> clice links against a custom LLVM library, which can be downloaded [here](https://github.com/clice-project/llvm-binary/releases/latest).

#### Run Unit Tests

```shell
./build/bin/unit_tests --test-dir="./tests/data" --resource-dir="<LLVM_INSTALL_PATH>/lib/clang/20"
```

#### Run Integration Tests

We recommend using [uv](https://github.com/astral-sh/uv) to manage Python dependencies and versions. If you prefer not to use uv, please refer to [pyproject.toml](./pyproject.toml) to install the required Python version and dependencies.

```shell
pytest -s --log-cli-level=INFO tests/integration --executable=./build/bin/clice --resource-dir="<LLVM_INSTALL_PATH>/lib/clang/20"
```

> `resource-dir` is clang's built-in header file directory.

***

### Supported Toolchains

- Clang
- MSVC (XMake only)

### Supported Platforms

- Windows (Release build only)
- Linux
- macOS
