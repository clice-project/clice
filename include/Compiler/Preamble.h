#pragma once

#include <string>
#include <vector>
#include <expected>

#include "llvm/ADT/StringRef.h"

namespace clice {

class ASTInfo;

struct CompilationParams;

struct PCHInfo {
    /// The path of the output PCH file.
    std::string path;

    /// The content used to build this PCH.
    std::string preamble;

    /// The command used to build this PCH.
    std::string command;

    /// All files involved in building this PCH.
    std::vector<std::string> deps;
};

/// Compute the preamble bound of given content. We just
/// run lex until we find first not directive.
std::uint32_t computePreambleBound(llvm::StringRef content);

/// Same as above, but return a group of bounds for chained PCH
/// building.
std::vector<uint32_t> computePreambleBounds(llvm::StringRef content);

}  // namespace clice
