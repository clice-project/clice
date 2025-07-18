#pragma once

#include "SourceLocation.h"
#include "clang/Lex/Token.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice {

struct LocalSourceRange {
    /// The begin position offset to the source file.
    uint32_t begin = -1;

    /// The end position offset to the source file.
    uint32_t end = -1;

    constexpr bool operator== (const LocalSourceRange& other) const = default;

    constexpr bool contains(uint32_t offset) const {
        return offset >= begin && offset <= end;
    }

    constexpr bool intersects(const LocalSourceRange& other) const {
        return begin <= other.end && end >= other.begin;
    }

    constexpr bool valid() const {
        return begin != -1 && end != -1;
    }
};

/// Get the content of the file with the given file ID.
llvm::StringRef getFileContent(const clang::SourceManager& SM, clang::FileID fid);

/// Get the length of the token at the given location. All SourceLocation instances in the clang
/// AST originate from the start position of tokens, which helps reduce memory usage. When token
/// length information is needed, a simple lexing operation based on the start position can be
/// performed.
std::uint32_t getTokenLength(const clang::SourceManager& SM, clang::SourceLocation location);

/// Get the spelling of the token at the given location.
llvm::StringRef getTokenSpelling(const clang::SourceManager& SM, clang::SourceLocation location);

/// A fake location could be used to calculate the token location offset when lexer
/// runs in raw mode.
inline clang::SourceLocation fakeLoc = clang::SourceLocation::getFromRawEncoding(1);

/// @brief Run `clang::Lexer` in raw mode and tokenize the content.
/// @param content The content to tokenize.
/// @param callback The callback to call for each token. Return false to break.
/// @param langOpts The language options to use. If not provided, lastest C++ standard is used.
void tokenize(llvm::StringRef content,
              llvm::unique_function<bool(const clang::Token&)> callback,
              bool ignoreComments = true,
              const clang::LangOptions* langOpts = nullptr);

}  // namespace clice
