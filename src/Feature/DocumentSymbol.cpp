#include "AST/FilterASTVisitor.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/DocumentSymbol.h"

namespace clice {

namespace {

using feature::document_symbol::DocumentSymbol;

/// Clangd's DocumentSymbol Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/FindSymbols.cpp#L286

/// Use DFS to traverse the AST and collect document symbols.
struct DocumentSymbolCollector : public FilteredASTVisitor<DocumentSymbolCollector> {

    using Base = FilteredASTVisitor<DocumentSymbolCollector>;

    using Storage = index::Shared<feature::document_symbol::Result>;

    /// DFS state stack.
    std::vector<std::pair<DocumentSymbol, clang::SourceLocation>> stack;

    /// Result of document symbols.
    Storage result;

    DocumentSymbolCollector(ASTInfo& AST, bool interestedOnly) :
        Base(AST, interestedOnly, std::nullopt) {}

    /// Entry a new AST node which may has some children nodes.
    void entry(DocumentSymbol symbol, clang::SourceLocation loc) {
        stack.push_back({std::move(symbol), loc});
    }

    /// Leave the current AST node.
    void leave() {
        stack.back().first.children.shrink_to_fit();
        auto last = std::move(stack.back());
        stack.pop_back();

        collect(std::move(last.first), last.second);
    }

    /// Collect a leaf node as the DocumentSymbol.
    void collect(DocumentSymbol symbol, clang::SourceLocation loc) {
        feature::document_symbol::Result* state;

        if(!stack.empty()) {
            state = &stack.back().first.children;
        } else {
            clang::FileID fileID = interestedOnly ? AST.getInterestedFile() : AST.getFileID(loc);
            state = &result[fileID];
        }

        state->push_back(std::move(symbol));
    }

    /// Mark the symbol as deprecated.
    void markDeprecated(DocumentSymbol& symbol) {
        symbol.tags.push_back(proto::SymbolTag{proto::SymbolTag::Deprecated});
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
            .kind = proto::SymbolKind::Namespace,
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
            .kind = proto::SymbolKind::Enum,
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
                .kind = proto::SymbolKind::EnumMember,
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
        symbol.kind = decl->isAbstract() ? proto::SymbolKind::Interface
                      : decl->isClass()  ? proto::SymbolKind::Class
                                         : proto::SymbolKind::Struct;

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
            .kind = proto::SymbolKind::Field,
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
            .kind = proto::SymbolKind::Function,
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
            .kind = decl->isConstexpr() ? proto::SymbolKind::Constant : proto::SymbolKind::Variable,
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

    static Storage collect(ASTInfo& AST, bool interestedOnly) {
        DocumentSymbolCollector collector{AST, interestedOnly};
        collector.TraverseTranslationUnitDecl(AST.tu());
        assert(collector.stack.empty() && "Unclosed scope to collect DocumentSymbol.");
        return std::move(collector.result);
    }
};

}  // namespace

namespace feature::document_symbol {

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#serverCapabilities
// ```
// 	/**
//	 * The server provides document symbol support.
//	 */
//	documentSymbolProvider?: boolean | DocumentSymbolOptions;
// ```
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentSymbolOptions
json::Value capability(json::Value clientCapabilities) {
    return json::Object{
        {"documentSymbolProvider", true},
    };
}

/// Get all document symbols in each file.
index::Shared<Result> documentSymbol(ASTInfo& AST) {
    return DocumentSymbolCollector::collect(AST, false);
}

Result documentSymbol(ASTInfo& AST, MainFileOnlyFlag _) {
    auto result = DocumentSymbolCollector::collect(AST, true);
    return std::move(result[AST.getInterestedFile()]);
}

proto::DocumentSymbol toLspType(const DocumentSymbol& result,
                                const SourceConverter& SC,
                                llvm::StringRef content) {
    proto::DocumentSymbol lspRes;

    lspRes.name = result.name;
    lspRes.detail = result.detail;
    lspRes.kind = result.kind;
    lspRes.tags = result.tags;

    lspRes.range = SC.toRange(result.range, content);
    lspRes.selectionRange = SC.toRange(result.selectionRange, content);

    lspRes.children.reserve(result.children.size());
    for(auto& child: result.children) {
        lspRes.children.push_back(toLspType(child, SC, content));
    }

    lspRes.children.shrink_to_fit();
    return lspRes;
}

proto::DocumentSymbolResult toLspResult(llvm::ArrayRef<DocumentSymbol> result,
                                        const SourceConverter& SC,
                                        llvm::StringRef content) {
    proto::DocumentSymbolResult lspRes;
    lspRes.reserve(result.size());

    for(auto& symbol: result) {
        lspRes.push_back(toLspType(symbol, SC, content));
    }

    lspRes.shrink_to_fit();
    return lspRes;
}

}  // namespace feature::document_symbol

}  // namespace clice
