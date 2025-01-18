#pragma once

#include <string>
#include <vector>

#include "Support/Error.h"

namespace clice {

class ASTInfo;

struct CompilationParams;

struct PCHInfo {
    /// The content used to build this PCH.
    std::string preamble;

    /// The command used to build this PCH.
    std::string command;

    /// The path of the output PCH file.
    std::string path;

    /// All files involved in building this PCH.
    std::vector<std::string> deps;
};


std::uint32_t computeBounds(llvm::StringRef content);

/// Computes the preamble bounds for the given content.
/// If the bounds are not provided explicitly, they will be calculated based on the content.
///
/// - If the header is empty, the bounds can be determined by lexing the source file.
/// - If the header is not empty, the preprocessor must be executed to compute the bounds.
std::uint32_t computeBounds(CompilationParams& params);

/// Build PCH from given file path and content.
llvm::Expected<ASTInfo> compile(CompilationParams& params, PCHInfo& out);

}  // namespace clice
