#include "Server/Indexer.h"
#include "Index/SymbolIndex.h"

namespace clice {

void Indexer::index(llvm::StringRef file, class ASTInfo& info) {
    auto indices = index::test(info);

    std::vector<IncludeChain> chains;

    llvm::DenseMap<clang::FileID, std::pair<uint32_t, uint32_t>> chainLocs;

    auto& srcMgr = info.srcMgr();

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

    for(auto& [fid, diretive]: info.directives()) {
        for(auto& include: diretive.includes) {
            
        }
    }

    auto tu = new TranslationUnit();
    tu->srcPath = file.str();

    for(auto& [fid, index]: indices) {
        auto srcPath = srcMgr.getFileEntryRefForID(fid)->getName();
        auto iter = headers.find(srcPath);
        if(iter == headers.end()) {
            auto header = new Header();
            header->srcPath = srcPath;

        } else {
        }
    }
}

}  // namespace clice
