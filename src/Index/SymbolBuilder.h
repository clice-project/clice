#pragma once

#include <clang/AST/DeclCXX.h>
#include <clang/Index/USRGeneration.h>

#include <AST/Utility.h>
#include <Compiler/Compiler.h>
#include <Index/Indexer.h>
#include <Support/FileSystem.h>

namespace clice::index {

namespace {

/// Responsible for building the symbol index.
class SymbolBuilder {
public:
    SymbolBuilder(clang::Sema& sema, clang::syntax::TokenBuffer& tokBuf) :
        sema(sema), context(sema.getASTContext()), srcMgr(context.getSourceManager()),
        tokBuf(tokBuf) {}

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
            locations[offset].begin.line = beginLoc.getLine() - 1;
            locations[offset].begin.column = beginLoc.getColumn() - 1;
            locations[offset].end.line = endLoc.getLine() - 1;
            locations[offset].end.column = endLoc.getColumn() - 1;
            locations[offset].file = addFile(srcMgr.getFileID(begin));
        }

        return LocationRef{offset};
    }

    friend class SymbolProxy;

    /// A proxy class for symbol reference for convenient usage.
    class SymbolProxy {
    public:
        explicit SymbolProxy(uint32_t offset, SymbolBuilder& builder) :
            offset(offset), builder(builder) {}

        SymbolProxy addOccurrence(LocationRef location) {
            assert(location.isValid() && "Invalid location");
            auto& occurrences = builder.index->occurrences;
            occurrences.emplace_back(location, SymbolRef{offset});
            return *this;
        }

        SymbolProxy addRelation(RelationKind kind,
                                LocationRef location,
                                SymOrLocRef symOrLoc = {}) {
            assert(location.isValid() && "Invalid location");
            auto& relations = builder.index->symbols[offset].relations;
            relations.emplace_back(kind, location, symOrLoc);
            return *this;
        }

        SymbolProxy addReference(LocationRef location) {
            RelationKind kind = RelationKind::Reference;
            return addRelation(kind, location);
        }

        SymbolProxy addDeclaration(LocationRef location) {
            RelationKind kind = RelationKind::Reference;
            kind.set(RelationKinds::Declaration);
            return addRelation(kind, location);
        }

        SymbolProxy addDefinition(LocationRef location) {
            RelationKind kind = RelationKind::Reference;
            kind.set(RelationKinds::Declaration);
            kind.set(RelationKinds::Definition);
            return addRelation(kind, location);
        }

        SymbolProxy addDeclarationOrDefinition(bool isDefinition, LocationRef location) {
            return isDefinition ? addDefinition(location) : addDeclaration(location);
        }

        SymbolProxy addTypeDefinition(clang::QualType type) {
            if(auto decl = declForType(type)) {
                /// FIXME: figure out Symbol lookup.
                if(auto location = builder.addLocation(decl->getLocation())) {
                    auto symbol = builder.addSymbol(decl);
                    return addRelation(RelationKind::TypeDefinition,
                                       location,
                                       SymOrLocRef{symbol.offset});
                }
            }
            return *this;
        }

    private:
        uint32_t offset;
        SymbolBuilder& builder;
    };

    /// Add a symbol to the index.
    SymbolProxy addSymbol(const clang::NamedDecl* decl) {
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
            clang::index::generateUSRForDecl(decl, USR);
            symbol.id = llvm::xxHash64(USR);
            symbol.name = decl->getNameAsString();
        }

        return SymbolProxy(offset, *this);
    }

    void indexTU(memory::Index& result, Compiler& compiler);

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    clang::SourceManager& srcMgr;
    clang::syntax::TokenBuffer& tokBuf;

    /// Index cache.
    using SourceRange = std::pair<clang::SourceLocation, clang::SourceLocation>;
    llvm::DenseMap<clang::FileID, std::size_t> fileCache;
    llvm::DenseMap<const clang::Decl*, std::size_t> symbolCache;
    llvm::DenseMap<SourceRange, std::size_t> locationCache;

    /// Index result.
    memory::Index* index = nullptr;
};

}  // namespace

}  // namespace clice::index

