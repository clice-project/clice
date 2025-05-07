#include "Compiler/AST.h"
#include "Index/USR.h"
#include "AST/Utility.h"

namespace clice {

const llvm::DenseSet<clang::FileID>& ASTInfo::files() {
    if(allFiles.empty()) {
        /// FIXME: handle preamble and embed file id.
        for(auto& [fid, diretive]: directives()) {
            for(auto& include: diretive.includes) {
                if(!include.skipped) {
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
            if(!include.skipped) {
                deps.try_emplace(getFilePath(include.fid));
            }
        }

        for(auto& hasInclude: diretive.hasIncludes) {
            if(hasInclude.fid.isValid()) {
                deps.try_emplace(getFilePath(hasInclude.fid));
            }
        }
    }

    std::vector<std::string> result;

    for(auto& deps: deps) {
        result.emplace_back(deps.getKey().str());
    }

    return result;
}

llvm::StringRef ASTInfo::getFilePath(clang::FileID fid) {
    assert(fid.isValid() && "Invalid fid");
    if(auto it = pathCache.find(fid); it != pathCache.end()) {
        return it->second;
    }

    auto entry = SM.getFileEntryRefForID(fid);
    assert(entry && "Invalid file entry");

    llvm::SmallString<128> path;

    /// Try to get the real path of the file.
    auto name = entry->getName();
    if(auto error = llvm::sys::fs::real_path(name, path)) {
        /// If failed, use the virtual path.
        path = name;
    }
    assert(!path.empty() && "Invalid file path");

    /// Allocate the path in the storage.
    auto size = path.size();
    auto data = pathStorage.Allocate<char>(size + 1);
    memcpy(data, path.data(), size);
    data[size] = '\0';

    auto [it, inserted] = pathCache.try_emplace(fid, llvm::StringRef(data, size));
    assert(inserted && "File path already exists");
    return it->second;
}

std::pair<clang::FileID, LocalSourceRange> ASTInfo::toLocalRange(clang::SourceRange range) {
    auto [begin, end] = range;
    assert(begin.isValid() && end.isValid() && "Invalid source range");
    assert(begin.isFileID() && end.isValid() && "Input source range should be a file range");

    if(begin == end) {
        auto [fid, offset] = getDecomposedLoc(begin);
        return {
            fid,
            {offset, offset + getTokenLength(SM, end)}
        };
    } else {
        auto [beginFID, beginOffset] = getDecomposedLoc(begin);
        auto [endFID, endOffset] = getDecomposedLoc(end);
        if(beginFID == endFID) {
            return {
                beginFID,
                {beginOffset, endOffset + getTokenLength(SM, end)}
            };
        } else {
            auto content = getFileContent(beginFID);
            return {
                beginFID,
                {beginOffset, static_cast<uint32_t>(content.size())}
            };
        }
    }
}

std::pair<clang::FileID, LocalSourceRange>
    ASTInfo::toLocalExpansionRange(clang::SourceRange range) {
    auto [begin, end] = range;
    if(begin == end) {
        return toLocalRange(getExpansionLoc(begin));
    } else {
        return toLocalRange(clang::SourceRange(getExpansionLoc(begin), getExpansionLoc(end)));
    }
}

index::SymbolID ASTInfo::getSymbolID(const clang::NamedDecl* decl) {
    uint64_t hash;
    auto iter = symbolHashCache.find(decl);
    if(iter != symbolHashCache.end()) {
        hash = iter->second;
    } else {
        llvm::SmallString<128> USR;
        index::generateUSRForDecl(decl, USR);
        hash = llvm::xxh3_64bits(USR);
        symbolHashCache.try_emplace(decl, hash);
    }
    return index::SymbolID{hash, getDeclName(decl)};
}

index::SymbolID ASTInfo::getSymbolID(const clang::MacroInfo* macro) {
    std::uint64_t hash;
    auto name = getTokenSpelling(SM, macro->getDefinitionLoc());
    auto iter = symbolHashCache.find(macro);
    if(iter != symbolHashCache.end()) {
        hash = iter->second;
    } else {
        llvm::SmallString<128> USR;
        index::generateUSRForMacro(name, macro->getDefinitionLoc(), SM, USR);
        hash = llvm::xxh3_64bits(USR);
        symbolHashCache.try_emplace(macro, hash);
    }
    return index::SymbolID{hash, name.str()};
}

}  // namespace clice
