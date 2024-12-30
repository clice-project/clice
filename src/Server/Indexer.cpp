#include "Server/Indexer.h"
#include "Index/SymbolIndex.h"

namespace clice {

void Indexer::index(llvm::StringRef file, class ASTInfo& info) {
    auto indices = index::test(info);

    std::vector<IncludeChain> chains;

    llvm::DenseMap<clang::FileID, std::pair<uint32_t, uint32_t>> chainLocs;

    auto& srcMgr = info.srcMgr();

    /// There are several include chains in source file.
    /// Along with the include chain, we can find all include files.
    /// So our basic idea is traverse all `FileID` emitted by `index`,
    /// Each `FileID` corresponds to a file, find each include chain
    /// and record the include chain in `chains` and the location of
    ///

    /// FIXME: flatten a 2D array into a 1D array
    /// consider index for PCH. determine main file.
    /// e.g. non self contain file.

    for(auto& [fid, index]: indices) {
        auto iter = chainLocs.find(fid);

        if(iter == chainLocs.end()) {
            auto current = fid;
            /// If we cannot find the file id, it means we need to create a new chain.
            uint32_t rows = chains.size();
            chains.emplace_back();
            auto& chain = chains.back();

            uint32_t cols = 0;

            auto includeLoc = srcMgr.getIncludeLoc(fid);

            while(includeLoc.isValid()) {
                auto loc = srcMgr.getPresumedLoc(includeLoc);
                chain.emplace_back(
                    SourceLocation{loc.getLine(), loc.getColumn(), loc.getFilename()});
                chainLocs[fid] = {rows, cols};
                cols += 1;
                current = loc.getFileID();
                includeLoc = loc.getIncludeLoc();
            }
        }
    }

    auto tu = new TranslationUnit();
    tu->srcPath = file.str();
    tu->chains = std::move(chains);

    for(auto& [fid, index]: indices) {
        auto srcPath = srcMgr.getFileEntryRefForID(fid)->getName();
        auto iter = headers.find(srcPath);
        if(iter == headers.end()) {
            auto header = new Header();
            header->srcPath = srcPath;
            header->contexts[tu] = {
                file.str(),
                llvm::ArrayRef(tu->chains[chainLocs[fid].first]).slice(chainLocs[fid].second)};
            headers[srcPath] = header;
            tu->headers.push_back(header);
        } else {
            iter->second->contexts[tu] = {
                file.str(),
                llvm::ArrayRef(tu->chains[chainLocs[fid].first]).slice(chainLocs[fid].second)};
            tu->headers.push_back(iter->second);
        }
    }
}

}  // namespace clice
