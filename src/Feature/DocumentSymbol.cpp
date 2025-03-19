#include "AST/FilterASTVisitor.h"
#include "Server/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/DocumentSymbol.h"

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
    void entry(DocumentSymbol symbol, clang::SourceLocation loc) {
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
        constexpr auto Default = "<anonymous namespace>";

        auto range = AST.toLocalRange(toLiteralRange(decl->getSourceRange())).second;
        DocumentSymbol symbol{
            .kind = SymbolKind::Namespace,
            .name = decl->isAnonymousNamespace() ? Default : decl->getNameAsString(),
            .range = range,
            .selectionRange = range,
        };

        entry(std::move(symbol), decl->getBeginLoc());
        bool res = Base::TraverseNamespaceDecl(decl);
        leave();

        return res;
    }

    bool TraverseEnumDecl(clang::EnumDecl* decl) {
        auto range = AST.toLocalRange(toLiteralRange(decl->getSourceRange())).second;
        DocumentSymbol symbol{
            .kind = SymbolKind::Enum,
            .name = decl->getNameAsString(),
            .range = range,
            .selectionRange = range,
        };

        entry(std::move(symbol), decl->getBeginLoc());
        bool res = Base::TraverseEnumDecl(decl);
        leave();

        return res;
    }

    bool VisitEnumDecl(const clang::EnumDecl* decl) {
        for(auto* etor: decl->enumerators()) {
            auto range = AST.toLocalRange(toLiteralRange(etor->getSourceRange())).second;
            DocumentSymbol symbol{
                .kind = SymbolKind::EnumMember,
                .name = etor->getNameAsString(),
                .range = range,
                .selectionRange = range,
            };

            // Show the initializer value as the detail.
            llvm::SmallString<32> sstr;
            sstr.append("= ");
            etor->getInitVal().toString(sstr);
            if(sstr.size() > 10)
                symbol.detail = "<initializer>";
            else
                symbol.detail = sstr.str().slice(0, sstr.size());

            if(etor->isDeprecated())
                markDeprecated(symbol);

            collect(std::move(symbol), etor->getBeginLoc());
        }

        return true;
    }

    bool TraverseCXXRecordDecl(clang::CXXRecordDecl* decl) {
        constexpr auto Default = "<anonymous struct>";

        auto range = AST.toLocalRange(toLiteralRange(decl->getSourceRange())).second;
        DocumentSymbol symbol{
            .range = range,
            .selectionRange = range,
        };
        symbol.kind = decl->isClass() ? SymbolKind::Class : SymbolKind::Struct;

        if(auto name = decl->getName(); !name.empty())
            symbol.name = name;
        else
            symbol.name = Default;

        entry(std::move(symbol), decl->getBeginLoc());
        bool res = Base::TraverseCXXRecordDecl(decl);
        leave();
        return res;
    }

    bool VisitFieldDecl(const clang::FieldDecl* decl) {
        auto range = AST.toLocalRange(toLiteralRange(decl->getSourceRange())).second;
        DocumentSymbol symbol{
            .kind = SymbolKind::Field,
            .name = decl->getNameAsString(),
            .range = range,
            .selectionRange = range,
        };
        symbol.detail = decl->getType().getAsString();

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol), decl->getBeginLoc());
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

    DocumentSymbol extractFunctionSymbol(const clang::FunctionDecl* decl) {
        auto local = AST.toLocalRange(toLiteralRange(decl->getSourceRange())).second;
        DocumentSymbol symbol{
            .kind = SymbolKind::Function,
            .name = decl->getNameAsString(),
            .range = local,
            .selectionRange = local,
        };
        symbol.detail = composeFuncSignature(decl);

        if(decl->isDeprecated())
            markDeprecated(symbol);

        return symbol;
    }

    bool TraverseFunctionDecl(clang::FunctionDecl* decl) {
        if(!decl)
            return true;

        if(auto spec = decl->getTemplateSpecializationInfo();
           spec && !spec->isExplicitSpecialization()) {
            return true;
        }

        entry(extractFunctionSymbol(decl), decl->getBeginLoc());
        bool res = Base::TraverseFunctionDecl(decl);
        leave();

        return res;
    }

    bool TraverseCXXMethodDecl(clang::CXXMethodDecl* decl) {
        if(!decl)
            return true;

        if(auto spec = decl->getTemplateSpecializationInfo();
           spec && !spec->isExplicitSpecialization()) {
            return true;
        }

        entry(extractFunctionSymbol(decl), decl->getBeginLoc());
        bool res = Base::TraverseCXXMethodDecl(decl);
        leave();

        return res;
    }

    bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl* decl) {
        if(decl->isImplicit())
            return true;

        entry(extractFunctionSymbol(decl), decl->getBeginLoc());
        bool res = Base::TraverseCXXConstructorDecl(decl);
        leave();
        return res;
    }

    bool TraverseCXXDestructorDecl(clang::CXXDestructorDecl* decl) {
        if(decl->isImplicit())
            return true;

        entry(extractFunctionSymbol(decl), decl->getBeginLoc());
        bool res = Base::TraverseCXXDestructorDecl(decl);
        leave();
        return res;
    }

    bool TraverseParmVarDecl(clang::ParmVarDecl* decl) {
        // Skip function parameters.
        return true;
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        /// Do not show local variables except static local variables.
        if(decl->isLocalVarDecl() && !decl->isStaticLocal())
            return true;

        auto local = AST.toLocalRange(toLiteralRange(decl->getSourceRange())).second;
        DocumentSymbol symbol{
            .kind = SymbolKind::Variable,
            .name = decl->getNameAsString(),
            .detail = decl->getType().getAsString(),
            .range = local,
            .selectionRange = local,
        };

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol), decl->getBeginLoc());
        return true;
    }

    static auto collect(ASTInfo& AST, bool interestedOnly) {
        DocumentSymbolCollector collector{AST, interestedOnly};
        collector.TraverseTranslationUnitDecl(AST.tu());
        /// assert(collector.stack.empty() && "Unclosed scope to collect DocumentSymbol.");
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
