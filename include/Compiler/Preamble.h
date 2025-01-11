#pragma once

#include <string>

namespace clice {

class ASTInfo;

struct CompilationParams;

struct PCHInfo {
    /// PCM file path.
    std::string path;

    std::string command;

    /// The content of source file used to build this PCM.
    std::string preamble;

    /// Files involved in building this PCM.
    std::vector<std::string> deps;

    clang::PreambleBounds bounds() const {
        /// We use '@' to mark the end of the preamble.
        bool endAtStart = preamble.ends_with('@');
        unsigned int size = preamble.size() - endAtStart;
        return {size, endAtStart};
    }
};

/// Build PCH from given file path and content.
llvm::Expected<ASTInfo> compile(CompilationParams& params, PCHInfo& out);

}  // namespace clice
