#include "Server/Indexer.h"
#include "Index/SymbolIndex.h"

namespace clice {

static uint32_t addIncludeChain(std::vector<SourceLocation>& locations,
                                llvm::DenseMap<clang::FileID, uint32_t>& cache,
                                clang::SourceManager& srcMgr,
                                clang::SourceLocation includeLoc) {
    if(includeLoc.isInvalid()) {
        return std::numeric_limits<uint32_t>::max();
    }

    assert(includeLoc.isFileID() && "Invalid include location");

    /// Should we use consider `line diretive`?
    auto presumed = srcMgr.getPresumedLoc(includeLoc, false);
    auto [iter, success] = cache.try_emplace(presumed.getFileID(), locations.size());
    auto index = iter->second;
    if(success) {
        locations.emplace_back(SourceLocation{
            .line = presumed.getLine(),
            .column = presumed.getColumn(),
            .filename = presumed.getFilename(),
        });

        /// Recursively add include chain. Note that `addIncludeChain` may resize
        /// the `locations`, so we use index instead of iterator.
        auto includeFile = addIncludeChain(locations, cache, srcMgr, presumed.getIncludeLoc());
        locations[index].includeFile = includeFile;
    }

    return index;
}

void Indexer::index(llvm::StringRef file, class ASTInfo& info) {
    auto indices = index::test(info);

    auto& srcMgr = info.srcMgr();

    std::vector<SourceLocation> locations;
    llvm::DenseMap<clang::FileID, uint32_t> cache;

    for(auto& [fid, diretive]: info.directives()) {
        for(auto& include: diretive.includes) {
            if(include.fid.isInvalid()) {
                continue;
            }

            addIncludeChain(locations, cache, srcMgr, srcMgr.getIncludeLoc(include.fid));
        }
    }

    /// FIXME: currently, if one include file generates empty symbol index,
    /// The output fileid will not contain it. But we need it for header context.
    /// imagine that user create a new empty file and wait for editing it. But
    /// we cannot find header context for it. This is really a problem.
    /// The resolution is using diretives in ASTInfo to gather all include files.

    /// FIXME: has include(...) need to be recorded in the header context.

    /// There are several include chains in source file.
    /// Along with the include chain, we can find all include files.
    /// So our basic idea is traverse all `FileID` emitted by `index`,
    /// Each `FileID` corresponds to a file, find each include chain
    /// and record the include chain in `chains` and the location of
    ///

    /// FIXME: flatten a 2D array into a 1D array
    /// consider index for PCH. determine main file.
    /// e.g. non self contain file.

    print("{}", json::serialize(locations));
}

}  // namespace clice
