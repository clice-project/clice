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

llvm::StringRef ASTInfo::getFilePath(clang::FileID fid) {
    assert(fid.isValid() && "Invalid fid");
    if(auto it = pathCache.find(fid); it != pathCache.end()) {
        return it->second;
    }

    auto entry = SM.getFileEntryRefForID(fid);
    assert(entry && "Invalid file entry");

    auto name = entry->getName();
    llvm::SmallString<128> path;

    /// Try to get the real path of the file.
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

    auto [it, inserted] = pathCache.try_emplace(fid, data, size);
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

}  // namespace clice
