#pragma once

#include <string>
#include <cstdint>

namespace clice {

/// 理解什么是 Context 非常重要
/// 对于 C++ 中的头文件来说，被不同的源文件包含，不同的源文件编译命令，
/// 不同的包含顺序都可能造成这个头文件产生的 AST 不同，进而对 LSP 的响应
/// 结果不同，对于 C++ 源文件来说则是不同的编译命令会产生不同的 AST
///
/// 不同于 clangd，clice 严格保留和跟踪文件上下文的概念，我们理所应当的支持
/// 非自包含的头文件，用户也可以自由的在不同上下文之间进行切换，或者使用所有
/// 上下文的综合结果

struct HeaderContext {
    /// Absolute file system path of the translation unit
    /// that includes this header.
    std::string tu;

    /// The canonical form of the compilation command used to
    /// compile the translation unit.
    std::string command;

    /// Zero-based ordinal of the include directive in the
    /// translation unit that introduces this header.
    std::uint32_t ordinal;
};

struct TUContext {
    /// The canonical form of the compilation command for
    /// this translation unit.
    std::string command;
};

}  // namespace clice
