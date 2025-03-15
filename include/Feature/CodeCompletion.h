#pragma once

#include <vector>
#include <cstdint>

#include "llvm/ADT/StringRef.h"

namespace clice {

struct CompilationParams;

namespace config {

struct CodeCompletionOption {};

};  // namespace config

namespace feature {

struct CodeCompletionItem {};

using CodeCompletionResult = std::vector<CodeCompletionItem>;

CodeCompletionResult codeCompletion(CompilationParams& params,
                                    const config::CodeCompletionOption& option);

}  // namespace feature

}  // namespace clice

