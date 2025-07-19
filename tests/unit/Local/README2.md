# What is clice

clice is a brand‑new C++ language server written in C++23 and built on the Clang AST. It aims to address some long‑standing issues in existing C++ language servers. Its design goal is to support as comprehensively as possible various C++ language features—such as header context.

> It is still in a period of rapid development, and any suggestions, requirements, or discussions are welcome!

# Why write a new language server?

**Why not contribute to an existing language server, and instead build a new one from scratch?**

The answer is multifaceted:
1. For VS Code users there are three main C++ language servers: `clangd`, `ccls`, and `cpp-tools`. Of these, `clangd` is the most mature, has the best support for the latest LSP standard, and performs exceptionally well on large codebases. Its success, however, was driven by Google dedicating engineers, [sam](https://github.com/sam-mccall), [kadircet](https://github.com/kadircet), and [hokein](https://github.com/hokein), from 2019 to 2022 to optimize it for massive C++ projects like Chromium. Since 2023, these engineers have moved on and `clangd` has largely gone unmaintained. Many pull requests in LLVM targeting clangd have stalled, and today only one volunteer, [Nathan Ridge](https://github.com/HighCommander4), occasionally answers community questions in his spare time. As a result, getting your patches reviewed and merged upstream is very difficult.
2. The clangd codebase has grown over many years and contains unnecessary coupling and incorrect assumptions. Supporting the new features I plan to introduce would require massive refactoring—likely more work than rewriting from scratch. I therefore decided to start fresh using a modern C++ standard, designing the architecture up front to maximize longevity. By embracing C++20’s major language features—`coroutines`, `ranges`, `concepts`, and `modules`—clice achieves both simplicity and power.
3. C++ is extremely complex, especially around template, and the Clang AST reflects that complexity. It’s all too easy to introduce subtle bugs, and there is relatively little documentation on the internals. By implementing a new C++ language server from the ground up, I forced myself to read and learn Clang’s source code in depth. To date, my familiarity with Clang’s internals far exceeds what’s strictly necessary to build a language server.

# Compared with clangd


# Roadmap


# Contribution