# Build from Source

## Supported Platforms

- Windows
- Linux
- MacOS

## Prerequisite

本小节介绍编译 clice 的前置依赖。

### Toolchain

- clang >= 19
- c++23 compitable standard library
  - MSVC STL >= 19.44(VS 2022 17.4)
  - GCC libstdc++ >= 14 
  - Clang libc++ >= 20
  
clice 使用 C++23 作为语言标准 ，请确保有可用的 clang 19 以及以上的编译器，以及兼容 C++23 的标准库。

> clice 暂时只能使用 clang 编译，在未来我们会改进这一点，使其能使用 gcc 和 msvc 编译。

### LLVM Libs

- 20.1.5 <= llvm libs < 21

由于 C++ 的语法太过复杂，自己编写一个新的 parser 是不现实的。clice 调用 clang 的 API 来 parse C++ 源文件获取 AST，这意味它需要链接 llvm/clang libs。另外由于 clice 使用了 clang 的私有头文件，这些私有头文件在 llvm 发布的 binary release 中是没有的，所以不能直接使用系统的 llvm package。

如果你能找到系统的 llvm package 对应的 llvm commit，将该 commit 下的如下三个文件

- `clang/lib/Sema/CoroutineStmtBuilder.h`
- `clang/lib/Sema/TypeLocBuilder.h` 
- `clang/lib/Sema/TreeTransform.h`

拷贝到 `LLVM_INSTALL_PATH/include/clang/Sema/` 中即可。

除了这种方法以外，还有两种办法获取 clice 所需的 llvm libs：

1. 使用我们提供的预编译版本

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
> 对于 debug 版本的 llvm libs，构建的时候我们开启了 address sanitizer，而 address sanitizer 依赖于 compiler rt，它对编译器版本十分敏感。所以如果使用 debug 版本，请确保你的 clang 的 compiler rt 版本和我们构建的时候**严格一致**。
> 
> - Windows 暂时无 debug 构建的 llvm libs，因为它不支持将 clang 构建为动态库，相关的进展可以在 [这里](https://discourse.llvm.org/t/llvm-is-buildable-as-a-windows-dll/87748) 找到
> - Linux 使用 clang20
> - MacOS 使用 homebrew llvm@20，一定不要使用 apple clang

2. 自己从头编译 llvm/clang

这是最推荐的方式，可以保证环境一致性，避免因为 ABI 不一致而导致的崩溃问题。我们提供了一个脚本，用于构建 clice 所需要的 llvm libs：[build-llvm-libs.py](https://github.com/clice-project/clice/blob/main/scripts/build-llvm-libs.py)。

```bash
$ cd llvm-project
$ python3 <clice>/scripts/build-llvm-libs.py debug
```

也可以参考 llvm 的官方构建教程 [Building LLVM with CMake](https://llvm.org/docs/CMake.html)。

## Building

在处理好前置依赖之后，可以开始构建 clice 了，我们提供 cmake/xmake 两种构建方式。

### CMake

下面是 clice 支持的 cmake 参数

- `LLVM_INSTALL_PATH` 用于指定 llvm libs 的安装路径
- `CLICE_ENABLE_TEST` 是否构建 clice 的单元测试

例如

```bach
$ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_INSTALL_PATH="./.llvm" -DCLICE_ENABLE_TEST=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
$ cmake --build build
```

### Xmake

使用如下的命令即可构建 clice

```bash
$ xmake f -c --dev=true --mode=debug --toolchain=clang --llvm="./.llvm" --enable_test=true
$ xmake build --all
```

> --llvm 是可选的，如果不指定的话，xmake 会自动下载我们编译好的预编译二进制

## Run Tests

clice 有两种形式的测试，单元测试和集成测试。

### 单元测试

```bash
$ ./build/bin/unit_tests --test-dir="./tests/data" --resource-dir="<LLVM_INSTALL_PATH>/lib/clang/20"
```

或者, 使用 xmake 启动单元测试:
```bash
$ xmake run --verbose unit_tests
```

# 集成测试

我们推荐使用 [uv](https://github.com/astral-sh/uv) 管理 python 依赖和版本。如果不想下载 uv，请参考 [pyproject.toml](./pyproject.toml) 下载所需的 python 版本和依赖。

```bash
$ pytest -s --log-cli-level=INFO tests/integration --executable=./build/bin/clice --resource-dir="<LLVM_INSTALL_PATH>/lib/clang/20"
```

> `resource-dir` 是 clang 的内置头文件文件夹

如果你使用 xmake 作为构建系统，可以直接通过 xmake 运行测试：

```shell
$ xmake test --verbose
$ xmake test --verbose integration_tests/default
```

在使用 xmake 构建和不使用 uv 的情况下, 启动 debug 模式的测试:

```shell
$ pip install pytest pytest-asyncio
$ xmake f -m debug && xmake build unit_tests

# 此处的 LLVM_INSTALL_DIR 在不同版本, 不同平台的 clice 中 sha 值可能不一样,
# 取决于对应的 xmake.lua 中 `package("llvm")` 一节 `set_versions` 的取值
$ LLVM_INSTALL_DIR=./build/.packages/l/llvm/20.1.5/0181167384bb4acb9e781210294c358d/lib/clang/20/ \
  pytest -s --log-cli-level=INFO tests/integration \
    --executable=./build/linux/x86_64/debug/clice \
    --resource-dir=$LLVM_INSTALL_DIR
```
