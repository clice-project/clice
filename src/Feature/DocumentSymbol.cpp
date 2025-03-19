#include "AST/FilterASTVisitor.h"
#include "AST/Utility.h"
#include "Compiler/Compilation.h"
#include "Feature/DocumentSymbol.h"

namespace clice::feature {

namespace {

/// Use DFS to traverse the AST and collect document symbols.
struct DocumentSymbolCollector : public FilteredASTVisitor<DocumentSymbolCollector> {
    using Base = FilteredASTVisitor<DocumentSymbolCollector>;

    struct SymbolFrame {
        DocumentSymbols symbols;
        DocumentSymbols* cursor = &symbols;
    };

    DocumentSymbolCollector(ASTInfo& AST, bool interestedOnly) :
        Base(AST, interestedOnly, std::nullopt) {}

    template <auto MF, typename Decl>
    bool collect(Decl* decl) {
        const clang::NamedDecl* ND = decl;

        auto [fid, selectionRange] = AST.toLocalRange(AST.getExpansionLoc(ND->getLocation()));

        auto& frame = interestedOnly ? result : sharedResult[fid];
        auto cursor = frame.cursor;

        /// Add new symbol.
        auto& symbol = frame.cursor->emplace_back();

        /// Adjust the node.
        frame.cursor = &symbol.children;

        bool res = (this->*MF)(decl);

        /// When all children node are set, go back to last node.
        (interestedOnly ? result : sharedResult[fid]).cursor = cursor;

        return res;
    }

#define COLLECT_DECL(type)                                                                         \
    bool Traverse##type(clang::type* decl) {                                                       \
        return collect<&Base::Traverse##type>(decl);                                               \
    }

    /// FIXME: Figure out which AST nodes we need to handle.
    COLLECT_DECL(NamespaceDecl);
    COLLECT_DECL(EnumDecl);
    COLLECT_DECL(EnumConstantDecl);
    COLLECT_DECL(RecordDecl);
    COLLECT_DECL(CXXRecordDecl);
    COLLECT_DECL(FieldDecl);
    COLLECT_DECL(FunctionDecl);
    COLLECT_DECL(CXXMethodDecl);
    COLLECT_DECL(CXXConversionDecl);
    COLLECT_DECL(VarDecl);
    COLLECT_DECL(ConceptDecl);

#undef COLLECT_DECL

private:
    SymbolFrame result;
    index::Shared<SymbolFrame> sharedResult;
};

}  // namespace

DocumentSymbols documentSymbols(ASTInfo& AST) {
    return {};
}

index::Shared<DocumentSymbols> indexDocumentSymbols(ASTInfo& AST) {
    return index::Shared<DocumentSymbols>{};
}

}  // namespace clice::feature
