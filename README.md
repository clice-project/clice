# clice: a powerful and efficient language server for c/c++

clice is a new language server for c/c++. It is still in the early stages of development, so just wait for a few months.


# Why write a new language server?

In VSCode, there are mainly three C++ plugins: cpp-tools, ccls, and clangd. Based on my experience, **clangd > ccls > cpptools**. However, this doesn't mean it has no shortcomings. In fact, compared to CLion's clangd (CLion uses its own private clangd branch), clangd has a significant gap.

Given this, why not contribute to clangd instead of writing a new one? The main reasons are:

Clangd was initially maintained by several Google employees: [Sam McCall](https://github.com/sam-mccall), [kadircet](https://github.com/kadircet), and [hokein](https://github.com/hokein). They mainly addressed Google's internal needs, and more complex requests from other users often had very low priority. Moreover, starting from 2023, these employees seem to have been reassigned to other projects. Clangd now has only one passionate, voluntary maintainer: [Nathan Ridge](https://github.com/HighCommander4). But he also has his own work to attend to, and his time is very limited—possibly only enough to answer some user queries in issues.

Some complex requirements often require large-scale modifications to clangd, and currently, there's no one available to review the related code. Such PRs might be shelved for a very long time. As far as I know, issues like adding support for C++20 modules to clangd—such as [Introduce initial support for C++20 Modules](https://github.com/llvm/llvm-project/pull/66462)—have been delayed for nearly a year. This pace is unacceptable to me. So the current situation is that making significant changes to clangd and merging them into the mainline is very difficult.

# Why should you choose clice?

## Better response inside template

```cpp
template<typename T>
void foo(std::vector<std::vector<T>> vec2) {
    vec2[0].^
}
```

`^` represents the location of cursor. clangd will give you no suggested completions here, while clice will give you the correct suggestions for `std::vector`. Not only for completion, but also for go-to-definition, find-references, etc. When it comes to handling templates, clice can always do better than clangd.

## Better performance and less memory usage

Code completion is really latency-sensitive. It could be really terrible if you have to wait for 1 second for each completion. Unfortunately, because C++ use header file to import other files, it is easy to get a large file with tens of thousands of lines. Parsing such a file could be very slow. clangd uses a technique called [PCH(Precompiled Header)](https://clang.llvm.org/docs/PCHInternals.html) to speed up parsing. When you edit a file, the content of headers actually doesn't change, so clangd will parse the headers, cache it and reuse it in the next parsing, which could save a lot of time. Such a cache is called preamble.

As you can see, preamble is very useful for frequently editing files. However clangd will build preamble for each file when you first open it. Actually, for a huge project with thousands of files, you may only edit a few files each time. For the rest of the files, you just need to use the readonly LSP features like go-to-definition, find-references, semantic token s, etc. In theory, it's possible to implement readonly features without any AST parsing, if you preindex the corresponding file. But clangd doesn't support this, all of its design is based on preamble. So, when you open a file, clangd will build preamble for all files, which is a waste of time and memory. But clice will distinguish between readonly files and writable files, and only build preamble for writable files. This could save a lot of time and memory.

## Better index format

clangd's index files cannot be distributed because it uses absolute paths. So you have to index the whole project every time you move it. clice uses relative paths in index files, so you can move the project freely. Besides, we have more detail information in index files, which could be used to implement readonly features without AST parsing.

## Support more semantic tokens

clangd only emits limited semantic tokens. It doesn't support literals, comments, keywords, etc. For vscode, highlighting for such lexical token is done by treesitter, which always gets wrong results in template context. clice will emit more kinds of semantic tokens, which could be used to implement better highlighting.

## Support non self contained files

What is a self contained file? A self contained file is a file that can be compiled independently. For example, suppose we have the following files:

```cpp
// a.h
struct A {};

struct B {
    A a;
};

// main.cpp
#include <a.h>
```

If we want to split the `B` struct into a separate file, normal way is to put `struct B` into `b.h` and include `a.h` in `b.h`. Then `b.h` is a self contained file.

```cpp
// a.h
struct A {};

// b.h
#include <a.h>

struct B {
    A a;
};

// main.cpp
#include <b.h>
```

Another way is to put `struct B` into `b.h` and doesn't include `a.h` in `b.h`. Then `b.h` is a non self contained file.

```cpp
// a.h
struct A {};

// b.h
struct B {
    A a;
};

// main.cpp
#include <a.h>
#include <b.h>
```


non self contained files are files that cannot be compiled independently. They are widely used in C++ projects, including libstdc++. clangd doesn't support non self contained files, so if you open the header file of libstdc++, you will get a lot of errors. But clice can handle this.

## Support more code actions

TODO: clice will provide more useful code actions.

## Support C++20 modules

TODO: clice will fully support C++20 modules.