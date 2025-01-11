#pragma once

#include <string>

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

/// Compute the preamble to build PCH with the given content.
std::string computePreamble(CompilationParams& params);

/// Build PCH from given file path and content.
llvm::Expected<ASTInfo> compile(CompilationParams& params, PCHInfo& out);

}  // namespace clice
