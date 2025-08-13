# Compilation

## Incremental Parsing

Every time you modify code, clice must re-parse the file. clice uses a mechanism called preamble to implement incremental compilation to speed up re-parsing. Preamble can be considered a special case of [Precompiled Header](https://clang.llvm.org/docs/PCHInternals.html) (embedded in source files). When opening a file, it builds the first few preprocessor directives at the beginning of the file (called preamble) into a PCH cache on disk. Later, when parsing, it can directly load the PCH file, thus skipping the first few preprocessor directives, which can greatly reduce the amount of code that needs to be re-parsed.

For example, for the following code:

```cpp
#include <iostream>

int main () {
    std::cout << "Hello world!" << std::endl;
}
```

The `iostream` header file has about 20,000 lines of code. clice will first build the line `#include <iostream>` into a PCH file, and after completion, use this PCH file to parse the subsequent code. This way, the amount of code that needs to be re-parsed later is reduced to just 5 lines instead of the original 20,000 lines, making it very fast. Unless you modify the code in the preamble section, which requires building a new preamble.

## Cancel Compilation
