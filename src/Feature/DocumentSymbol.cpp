#include "AST/FilterASTVisitor.h"
#include "AST/Utility.h"
#include "Compiler/Compilation.h"
#include "Feature/DocumentSymbol.h"
#include "Server/SourceConverter.h"

namespace clice::feature {

namespace {

/// Use DFS to traverse the AST and collect document symbols.
struct DocumentSymbolCollector : public FilteredASTVisitor<DocumentSymbolCollector> {
    using Base = FilteredASTVisitor<DocumentSymbolCollector>;

    /// DFS state stack.
    /// std::vector<std::pair<DocumentSymbol, clang::SourceLocation>> stack;
    index::Shared<std::vector<DocumentSymbol>> result;

    DocumentSymbolCollector(ASTInfo& AST, bool interestedOnly) :
        Base(AST, interestedOnly, std::nullopt) {}

    /// Entry a new AST node which may has some children nodes.
    void entry(const clang::NamedDecl* decl) {
        /// stack.push_back({std::move(symbol), loc});
    }

    /// Leave the current AST node.
    void leave() {
        /// stack.back().first.children.shrink_to_fit();
        /// auto last = std::move(stack.back());
        /// stack.pop_back();
        /// collect(std::move(last.first), last.second);
    }

    /// Collect a leaf node as the DocumentSymbol.
    void collect(DocumentSymbol symbol, clang::SourceLocation loc) {
        // document_symbol::Result* state;
        //
        // if(!stack.empty()) {
        //    state = &stack.back().first.children;
        //} else {
        //    clang::FileID fileID = interestedOnly ? AST.getInterestedFile() : AST.getFileID(loc);
        //    state = &result[fileID];
        //}
        //
        // state->push_back(std::move(symbol));
    }

    void addDocumentSymbol(const clang::NamedDecl* decl) {
        auto [_, selectionRange] = AST.toLocalRange(AST.getExpansionLoc(decl->getLocation()));
        auto [begin, end] = decl->getSourceRange();
        begin = AST.getExpansionLoc(begin);
        end = AST.getExpansionLoc(end);
        auto [__, range] = AST.toLocalRange(clang::SourceRange(begin, end));

        DocumentSymbol symbol{
            .kind = SymbolKind::from(decl),
            .name = getDeclName(decl),
            .detail = "",
            .selectionRange = selectionRange,
            .range = range,
        };
    }

    /// Mark the symbol as deprecated.
    void markDeprecated(DocumentSymbol& symbol) {
        /// symbol.tags.push_back(proto::SymbolTag{proto::SymbolTag::Deprecated});
    }

    /// For a given location, it could be one of SpellingLoc or ExpansionLoc (from macro expansion).
    /// So take literal location as the result for macro.
    clang::SourceRange toLiteralRange(clang::SourceRange range) {
        auto takeLocation = [this](clang::SourceLocation loc) {
            return loc.isMacroID() ? AST.getExpansionLoc(loc) : loc;
        };

        auto [begin, end] = range;
        return {takeLocation(begin), takeLocation(end)};
    }

    bool TraverseNamespaceDecl(clang::NamespaceDecl* decl) {
        entry(decl);
        bool res = Base::TraverseNamespaceDecl(decl);
        leave();

        return res;
    }

    bool TraverseEnumDecl(clang::EnumDecl* decl) {
        entry(decl);
        bool res = Base::TraverseEnumDecl(decl);
        leave();

        return res;
    }

    bool TraverseCXXRecordDecl(clang::CXXRecordDecl* decl) {
        entry(decl);
        bool res = Base::TraverseCXXRecordDecl(decl);
        leave();
        return res;
    }

    bool VisitFieldDecl(const clang::FieldDecl* decl) {
        addDocumentSymbol(decl);
        return true;
    }

    static std::string composeFuncSignature(const clang::FunctionDecl* decl) {
        std::string signature = decl->getReturnType().getAsString();

        signature += " (";
        for(auto* param: decl->parameters()) {
            signature += param->getType().getAsString();
            signature += ",";
        }
        if(!decl->param_empty())
            signature.pop_back();
        signature += ")";

        return signature;
    }

    bool TraverseFunctionDecl(clang::FunctionDecl* decl) {
        entry(decl);
        bool res = Base::TraverseFunctionDecl(decl);
        leave();
        return res;
    }

    bool TraverseCXXMethodDecl(clang::CXXMethodDecl* decl) {
        entry(decl);
        bool res = Base::TraverseCXXMethodDecl(decl);
        leave();
        return res;
    }

    bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl* decl) {
        entry(decl);
        bool res = Base::TraverseCXXConstructorDecl(decl);
        leave();
        return res;
    }

    bool TraverseCXXDestructorDecl(clang::CXXDestructorDecl* decl) {
        entry(decl);
        bool res = Base::TraverseCXXDestructorDecl(decl);
        leave();
        return res;
    }

    bool TraverseVarDecl(const clang::VarDecl* decl) {
        addDocumentSymbol(decl);
        return true;
    }

    static auto collect(ASTInfo& AST, bool interestedOnly) {
        DocumentSymbolCollector collector{AST, interestedOnly};
        collector.TraverseTranslationUnitDecl(AST.tu());
        return std::move(collector.result);
    }
};

}  // namespace

std::vector<DocumentSymbol> documentSymbol(ASTInfo& AST) {
    return {};
}

index::Shared<std::vector<DocumentSymbol>> indexDocumentSymbol(ASTInfo& AST) {
    return index::Shared<std::vector<DocumentSymbol>>{};
}

}  // namespace clice::feature
