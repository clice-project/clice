#include "Server/Scheduler.h"

namespace clice {

async::Task<ASTInfo> Scheduler::build(llvm::StringRef file, llvm::StringRef content) {
    if(auto it = PCHs.find(file); it != PCHs.end()) {
        auto bound = computePreambleBound(content);
        auto preamble = content.substr(0, bound);
    }
}

}  // namespace clice
