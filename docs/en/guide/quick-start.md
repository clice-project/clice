# Quick Start

## Editor Plugins

clice implements the [Language Server Protocol](https://microsoft.github.io/language-server-protocol). Any editor that supports this protocol can theoretically work with clice to provide features like `code completion`, `diagnostics`, `go-to-definition`, and more.

However, beyond the standard protocol, clice also supports some protocol extensions. For better handling of these protocol extensions and better integration with editors, using clice plugins in specific editors is often a better choice. Most of them work out of the box and support clice's protocol extensions.

### Visual Studio Code

### Vim/Neovim

### Others

Other editors don't have available clice plugins yet (contributions welcome!). To use clice in them, please install clice yourself and refer to the specific editor's documentation on how to use a language server.

## Installation

If your editor plugin handles clice's download, you can skip this step.

### Download Prebuilt Binary

Download clice binary version through the Release page.

### Build from Source

Build clice from source yourself. For specific steps, refer to [build](../dev/build.md).

## Project Setup

For clice to correctly understand your code (e.g., find header file locations), you need to provide clice with a `compile_commands.json` file, also known as a [compilation database](https://clang.llvm.org/docs/JSONCompilationDatabase.html). The compilation database provides compilation options for each source file.

### CMake

For build systems using cmake, add the `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` option during build, for example:

```cmake
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

This will generate a `compile_commands.json` file in the `build` directory.

::: warning
Note: This option only works when cmake's generator is set to makefile and ninja. For other generators, this option will be ignored, meaning the compilation database cannot be generated.
:::

### Bazel

TODO:

### Visual Studio

TODO:

### Makefile

TODO:

### Xmake

### Others

For any other build system, you can try using [bear](https://github.com/rizsotto/Bear) or [scan-build](https://github.com/rizsotto/scan-build) to intercept compilation commands and obtain the compilation database (no guarantee of success). We plan to write a **new tool** in the future that captures compilation commands through a fake compiler approach.
