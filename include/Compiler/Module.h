#pragma once

#include <string>
#include <vector>

#include "Support/Struct.h"
#include "Support/Error.h"

namespace clice {

class ASTInfo;

struct CompilationParams;

struct ModuleInfo {
    /// Whether this module is an interface unit.
    /// i.e. has export module declaration.
    bool isInterfaceUnit = false;

    /// Module name.
    std::string name;

    /// Dependent modules of this module.
    std::vector<std::string> mods;
};

inherited_struct(PCMInfo, ModuleInfo) {
    /// PCM file path.
    std::string path;

    /// Source file path.
    std::string srcPath;

    /// Files involved in building this PCM(not include module).
    std::vector<std::string> deps;
};

/// Run the preprocessor to scan the given module unit to
/// collect its module name and dependencies.
llvm::Expected<ModuleInfo> scanModule(CompilationParams& params);

/// Build PCM from given file path and content.
llvm::Expected<ASTInfo> compile(CompilationParams& params, PCMInfo& out);

}  // namespace clice
