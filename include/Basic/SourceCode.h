#pragma once

#include "clang/Basic/SourceLocation.h"

namespace clice {

/// Get the content of the file with the given file ID.
llvm::StringRef getFileContent(const clang::SourceManager& SM, clang::FileID fid);

/// Get the length of the token at the given location. All SourceLocation instances in the clang
/// AST originate from the start position of tokens, which helps reduce memory usage. When token
/// length information is needed, a simple lexing operation based on the start position can be
/// performed.
std::uint32_t getTokenLength(const clang::SourceManager& SM, clang::SourceLocation location);

/// Get the spelling of the token at the given location.
llvm::StringRef getTokenSpelling(const clang::SourceManager& SM, clang::SourceLocation location);

}  // namespace clice
