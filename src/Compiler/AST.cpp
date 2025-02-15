#include "Compiler/AST.h"

namespace clice {

const llvm::DenseSet<clang::FileID>& ASTInfo::files() {
    if(allFiles.empty()) {
        /// FIXME: handle preamble and embed file id.
        for(auto& [fid, diretive]: directives()) {
            for(auto& include: diretive.includes) {
                if(include.fid.isValid()) {
                    allFiles.insert(include.fid);
                }
            }
        }
        allFiles.insert(srcMgr().getMainFileID());
    }
    return allFiles;
}

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
