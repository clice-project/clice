#include "Compiler/AST.h"

namespace clice {

std::vector<std::string> ASTInfo::deps() {
    llvm::StringSet<> deps;

    /// FIXME: consider `#embed` and `__has_embed`.

    for(auto& [fid, diretive]: directives()) {
        for(auto& include: diretive.includes) {
            if(include.fid.isValid()) {
                auto entry = srcMgr().getFileEntryRefForID(include.fid);
                assert(entry && "Invalid file entry");
                deps.try_emplace(entry->getName());
            }
        }

        for(auto& hasInclude: diretive.hasIncludes) {
            deps.try_emplace(hasInclude.path);
        }
    }

    std::vector<std::string> result;

    for(auto& deps: deps) {
        result.emplace_back(deps.getKey().str());
    }

    return result;
}

}  // namespace clice
