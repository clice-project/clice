# Header Context

For clangd to work properly, users often need to provide a `compile_commands.json` file (hereinafter referred to as CDB file). The basic compilation unit of C++'s traditional compilation model is a source file (e.g., `.c` and `.cpp` files), where `#include` simply pastes and copies the content of header files to the corresponding position in the source file. The aforementioned CDB file stores the compilation commands corresponding to each source file. When you open a source file, clangd will use its corresponding compilation command in the CDB to compile this file.

Naturally, there's a question: if the CDB file only contains compilation commands for source files and not header files, how does clangd handle header files? clangd treats header files as source files, and then, according to some rules, such as using the compilation command of the source file in the corresponding directory as the compilation command for that header file. This model is simple and effective, but it ignores some situations.

Since header files are part of source files, there will be cases where their content differs depending on the content that precedes them in the source file. For example:

```cpp
// a.h
#ifdef TEST
struct X { ... };
#else
struct Y { ... };
#endif

// b.cpp
#define TEST
#include "a.h"

// c.cpp
#include "a.h"
```

Obviously, `a.h` has different states in `b.cpp` and `c.cpp` - one defines `X` and the other defines `Y`. If we simply treat `a.h` as a source file, then only `Y` can be seen.

A more extreme case is non-self-contained header files, for example:

```cpp
// a.h
struct Y {
    X x;
};

// b.cpp
struct X {};
#include "a.h"
```

`a.h` itself cannot be compiled, but when embedded in `b.cpp`, it compiles normally. In this case, clangd will report an error in `a.h`, unable to find the definition of `X`. Obviously, this is because it treats `a.h` as an independent source file. There are many such header files in libstdc++ code, and some popular C++ header-only libraries also have such code, which clangd currently cannot handle.

clice will support **header context**, supporting automatic and user-initiated switching of header file states, and of course will also support non-self-contained header files. We want to achieve the following effect, using the first piece of code as an example. When you jump from `b.cpp` to `a.h`, use `b.cpp` as the context for `a.h`. Similarly, when you jump from `c.cpp` to `a.h`, use `c.cpp` as the context for `a.h`.
