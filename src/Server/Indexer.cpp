#include "Server/Indexer.h"
#include "Index/SymbolIndex.h"

namespace clice {

static uint32_t addIncludeChain(std::vector<SourceLocation>& locations,
                                llvm::DenseMap<clang::FileID, uint32_t>& cache,
                                clang::SourceManager& srcMgr,
                                clang::FileID fid) {
    if(fid.isInvalid()) {
        return std::numeric_limits<uint32_t>::max();
    }

    /// FIXME: Should we consider `#line` diretive?
    auto [iter, success] = cache.try_emplace(fid, locations.size());
    auto index = iter->second;

    auto includeLoc = srcMgr.getIncludeLoc(fid);

    if(includeLoc.isInvalid()) {
        return index;
    }

    auto presumed = srcMgr.getPresumedLoc(includeLoc, false);
    if(success) {
        println("insert {} : {}", presumed.getFileID().getHashValue(), index);
        locations.emplace_back(SourceLocation{
            .line = presumed.getLine(),
            .column = presumed.getColumn(),
            .filename = presumed.getFilename(),
        });

        /// Recursively add include chain. Note that `addIncludeChain` may resize
        /// the `locations`, so we use index instead of iterator.
        auto includeFile = addIncludeChain(locations, cache, srcMgr, presumed.getFileID());
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

            addIncludeChain(locations, cache, srcMgr, include.fid);
        }
    }

    TranslationUnit* tu;
    auto name = srcMgr.getFileEntryRefForID(srcMgr.getMainFileID())->getName();
    auto iter = this->translationUnits.find(name);
    if(iter != this->translationUnits.end()) {
        tu = iter->second;
        /// If we reindex the file, we need to remove the old headers.
        for(auto header: tu->headers) {
            header->contexts.erase(tu);
        }
    } else {
        tu = new TranslationUnit();
        this->translationUnits.try_emplace(name, tu);
    }

    tu->srcPath = name;
    llvm::outs() << json::serialize(locations) << "\n";
    tu->locations = std::move(locations);

    for(auto& [fid, index]: indices) {
        if(fid == srcMgr.getMainFileID()) {
            continue;
        }

        Header* header;

        /// FIXME: Handle builtin file,
        if(!srcMgr.getFileEntryRefForID(fid)) {
            continue;
        }

        auto name = srcMgr.getFileEntryRefForID(fid)->getName();
        auto iter = this->headers.find(name);
        if(iter != this->headers.end()) {
            header = iter->second;
        } else {
            header = new Header();
            this->headers.try_emplace(name, header);
        }

        /// We already remove context before.
        assert(cache.contains(fid) && "Invalid file id");
        header->srcPath = name;
        header->contexts[tu].emplace_back(Header::Context{
            .indexPath = "",
            .includeLoc = cache[fid],
        });

        tu->headers.emplace_back(header);
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
}

void Indexer::load() {
    /// FIXME: Parse json ... and load it.
}

void Indexer::save() {
    json::Array tus;
    for(auto& [name, tu]: this->translationUnits) {
        tus.emplace_back(json::Object{
            {"srcPath",   tu->srcPath                   },
            {"indexPath", tu->indexPath                 },
            {"locations", json::serialize(tu->locations)},
        });
    };

    json::Array headers;
    for(auto& [name, header]: this->headers) {
        auto contexts = json::Object();
        for(auto& [tu, context]: header->contexts) {
            contexts.try_emplace(tu->srcPath, json::serialize(context));
        }

        headers.emplace_back(json::Object{
            {"srcPath",  header->srcPath    },
            {"contexts", std::move(contexts)},
        });
    }

    json::Object obj;
    obj.try_emplace("tus", std::move(tus));
    obj.try_emplace("headers", std::move(headers));

    std::error_code ec;
    llvm::raw_fd_ostream os("build.json", ec, llvm::sys::fs::OF_Text);
}

Indexer::~Indexer() {
    for(auto& [_, tu]: this->translationUnits) {
        delete tu;
    }

    for(auto& [_, header]: this->headers) {
        delete header;
    }
}

}  // namespace clice

