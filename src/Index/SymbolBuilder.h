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
            llvm::SmallString<128> path;
            auto entry = srcMgr.getFileEntryRefForID(id);
            if(!entry) {
                llvm::errs() << "File entry not found\n";
                std::terminate();
            }

            if(auto error = fs::real_path(entry->getName(), path)) {
                llvm::errs() << error.message() << "\n";
                std::terminate();
            }

            auto include = addLocation(srcMgr.getIncludeLoc(id));

            /// Note that addLocation may invalidate the reference to files.
            /// So we use the offset to access the file.
            files[offset].path = path.str();
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
            index->locations.emplace_back();
            Location& location = index->locations.back();

            auto beginLoc = srcMgr.getPresumedLoc(begin);
            auto endTok = clang::Lexer::getLocForEndOfToken(end, 0, srcMgr, sema.getLangOpts());
            auto endLoc = srcMgr.getPresumedLoc(endTok);

            location.begin.line = beginLoc.getLine();
            location.begin.column = beginLoc.getColumn();
            location.end.line = endLoc.getLine();
            location.end.column = endLoc.getColumn();
            location.file = addFile(srcMgr.getFileID(begin));
        }

        return LocationRef{offset};
    }

    friend class SymbolProxy;

    /// A proxy class for symbol reference for convenient usage.
    class SymbolProxy {
    public:
        explicit SymbolProxy(uint32_t offset, SymbolBuilder& builder) :
            offset(offset), builder(builder) {}

        /// TODO: ...

        SymbolProxy addOccurrence(LocationRef location) {
            llvm::outs() << location.offset << "\n";
            if(location.isInvalid()) {
                std::terminate();
            }
            auto& occurrences = builder.index->occurrences;
            occurrences.emplace_back(location, SymbolRef{offset});
            return *this;
        }

        SymbolProxy addDeclarationOrDefinition(bool isDefinition, LocationRef location) {
            if(location.isInvalid()) {
                std::terminate();
            }
            return *this;
        }

        SymbolProxy addDefinition(LocationRef location) {
            if(location.isInvalid()) {
                std::terminate();
            }
            return *this;
        }

        SymbolProxy addDeclaration(LocationRef location) {
            if(location.isInvalid()) {
                std::terminate();
            }
            return *this;
        }

        SymbolProxy addReference(LocationRef location) {
            if(location.isInvalid()) {
                std::terminate();
            }
            return *this;
        }

        SymbolProxy addTypeDefinition(clang::QualType type) {
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

