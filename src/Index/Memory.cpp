#include "Index/Memory.h"

namespace clice::index {

namespace {

class IndexBuilder : public SemanticVisitor<IndexBuilder> {
public:
    IndexBuilder(ASTInfo& info) : SemanticVisitor(info) {}

    /// Add a file to the index.
    FileRef addFile(clang::FileID id) {
        auto& files = index->files;

        /// Try to insert the file into the cache.
        auto [iter, success] = fileCache.try_emplace(id, files.size());
        uint32_t offset = iter->second;

        /// If insertion is successful, add the file to the index.
        if(success) {
            files.emplace_back();

            /// FIXME: handle error.
            auto entry = srcMgr.getFileEntryRefForID(id);
            if(!entry) {
                llvm::errs() << "File entry not found\n";
                std::terminate();
            }

            /// FIXME: resolve the real path(consider real path).
            // llvm::SmallString<128> path;
            // if(auto error = fs::real_path(entry->getName(), path)) {
            //     llvm::errs() << error.message() << "\n";
            //     std::terminate();
            // }

            auto include = addLocation(srcMgr.getIncludeLoc(id));

            /// Note that addLocation may invalidate the reference to files.
            /// So we use the offset to access the file.
            files[offset].path = entry->getName();
            files[offset].include = include;
        }

        return FileRef{offset};
    }

    /// Add a symbol to the index.
    LocationRef addLocation(clang::SourceRange range) {
        auto [begin, end] = range;

        /// FIXME: handle macro locations.
        if(begin.isInvalid() || end.isInvalid() || begin.isMacroID() || end.isMacroID()) {
            return LocationRef{};
        }

        /// Try to insert the location into the cache.
        auto [iter, success] = locationCache.try_emplace({begin, end}, index->locations.size());
        uint32_t offset = iter->second;

        /// If insertion is successful, add the location to the index.
        if(success) {
            auto& locations = index->locations;
            locations.emplace_back();

            auto beginLoc = srcMgr.getPresumedLoc(begin);
            auto endTok = clang::Lexer::getLocForEndOfToken(end, 0, srcMgr, sema.getLangOpts());
            auto endLoc = srcMgr.getPresumedLoc(endTok);

            /// FIXME: position encoding.
            locations[offset].range.start.line = beginLoc.getLine() - 1;
            locations[offset].range.start.character = beginLoc.getColumn() - 1;
            locations[offset].range.end.line = endLoc.getLine() - 1;
            locations[offset].range.end.character = endLoc.getColumn() - 1;
            locations[offset].file = addFile(srcMgr.getFileID(begin));
        }

        return LocationRef{offset};
    }

    /// Add a symbol to the index.
    void addSymbol(const clang::NamedDecl* decl) {
        if(!decl) {
            llvm::errs() << "Invalid decl\n";
            std::terminate();
        }

        if(auto instantiated = instantiatedFrom(decl)) {
            decl = instantiated;
        }

        auto& symbols = index->symbols;
        auto canonical = decl->getCanonicalDecl();

        /// Try to insert the symbol into the cache.
        auto [iter, success] = symbolCache.try_emplace(canonical, symbols.size());
        uint32_t offset = iter->second;

        /// If insertion is successful, add the symbol to the index.
        if(success) {
            memory::Symbol& symbol = symbols.emplace_back();
            llvm::SmallString<128> USR;
            /// clang::index::generateUSRForDecl(canonical, USR);
            symbol.id = llvm::xxHash64(USR);
            symbol.name = decl->getNameAsString();
        }

        /// return SymbolProxy(offset, *this);
    }

public:
    using SemanticVisitor::handleOccurrence;

    void handleOccurrence(const clang::Decl* decl,
                          clang::SourceRange range,
                          RelationKind kind = RelationKind::Invalid) {}

private:
    /// Index cache.
    using SourceRange = std::pair<clang::SourceLocation, clang::SourceLocation>;
    llvm::DenseMap<clang::FileID, std::size_t> fileCache;
    llvm::DenseMap<const clang::Decl*, std::size_t> symbolCache;
    llvm::DenseMap<SourceRange, std::size_t> locationCache;

    /// Index result.
    memory::Index* index = nullptr;
};

}  // namespace

/// Index the AST information.
memory::Index index(ASTInfo& info) {
    memory::Index index = {};
    IndexBuilder builder(info);
    builder.TraverseAST(info.context());
    return index;
}

/// Convert `memory::Index` to JSON.
json::Value toJSON(const memory::Index& index) {
    json::Value value = {};
    return value;
}

}  // namespace clice::index
