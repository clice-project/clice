# clice: a powerful and efficient language server for c/c++

clice is a new language server for c/c++. It is still in the early stages of development, so just wait for a few months.

Current progress is recorded in issues, feel free to share your ideas and suggestions!

# Why write a new language server?

In VSCode, there are mainly three C++ plugins: cpp-tools, ccls, and clangd. Based on my experience, **clangd > ccls > cpptools**. However, this doesn't mean it has no shortcomings. In fact, compared to CLion's clangd (CLion uses its own private clangd branch), clangd has a significant gap.

Given this, why not contribute to clangd instead of writing a new one? The main reasons are:

Clangd was initially maintained by several Google employees: [Sam McCall](https://github.com/sam-mccall), [kadircet](https://github.com/kadircet), and [hokein](https://github.com/hokein). They mainly addressed Google's internal needs, and more complex requests from other users often had very low priority. Moreover, starting from 2023, these employees seem to have been reassigned to other projects. Clangd now has only one passionate, voluntary maintainer: [Nathan Ridge](https://github.com/HighCommander4). But he also has his own work to attend to, and his time is very limited—possibly only enough to answer some user queries in issues.

Some complex requirements often require large-scale modifications to clangd, and currently, there's no one available to review the related code. Such PRs might be shelved for a very long time. As far as I know, issues like adding support for C++20 modules to clangd—such as [Introduce initial support for C++20 Modules](https://github.com/llvm/llvm-project/pull/66462) — have been delayed for nearly a year. This pace is unacceptable to me. So the current situation is that making significant changes to clangd and merging them into the mainline is very difficult.

# Why should you choose clice?

- Support non self contained files
- Support more code actions
- Support full C++20 named module
- Support more semantic tokens
- Better response inside template
- Better performance and less memory usage
- Better index format
