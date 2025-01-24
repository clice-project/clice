#include "Basic/SourceConverter.h"
#include "Feature/DocumentSymbol.h"
#include "Compiler/Compilation.h"

namespace clice {

namespace {

using feature::document_symbol::DocumentSymbol;

/// Clangd's DocumentSymbol Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/FindSymbols.cpp#L286

/// Use DFS to traverse the AST and collect document symbols.
struct DocumentSymbolCollector : clang::RecursiveASTVisitor<DocumentSymbolCollector> {

    using Base = clang::RecursiveASTVisitor<DocumentSymbolCollector>;

    using Storage = index::Shared<feature::document_symbol::Result>;

    const clang::SourceManager& src;

    const SourceConverter& cvtr;

    /// DFS state stack.
    std::vector<std::pair<DocumentSymbol, clang::SourceLocation>> stack;

    /// Result of document symbols.
    Storage result;

    /// True if only collect symbols in the main file.
    const bool onlyMain;

    /// Main file ID, available if `onlyMain` is true.
    const clang::FileID mainID;

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

        if(!stack.empty())
            state = &stack.back().first.children;
        else
            state = &result[onlyMain ? mainID : src.getFileID(loc)];

        state->push_back(std::move(symbol));
    }

    /// Mark the symbol as deprecated.
    void markDeprecated(DocumentSymbol& symbol) {
        symbol.tags.push_back(proto::SymbolTag{proto::SymbolTag::Deprecated});
    }

    /// For a given location, it could be one of SpellingLoc or ExpansionLoc (from macro expansion).
    /// So take literal location as the result for macro.
    clang::SourceRange toLiteralRange(clang::SourceRange range) {
        auto [begin, end] = range;

        auto takeLocation = [this](clang::SourceLocation loc) {
            return !loc.isMacroID() ? loc : src.getExpansionLoc(loc);
        };

        return {takeLocation(begin), takeLocation(end)};
    }

    bool TraverseDecl(clang::Decl* decl) {
        if(!decl)
            return true;

        if(!llvm::isa<clang::NamedDecl>(decl) || decl->isImplicit())
            return true;

        if(auto loc = decl->getLocation(); loc.isInvalid() || (onlyMain && !src.isInMainFile(loc)))
            return true;

        if(auto* ctsd = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
            if(!ctsd->isExplicitInstantiationOrSpecialization())
                return true;
        }

        if(auto* vtsd = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
            if(!vtsd->isExplicitInstantiationOrSpecialization())
                return true;
        }

        return Base::TraverseDecl(decl);
    }

    bool TraverseNamespaceDecl(clang::NamespaceDecl* decl) {
        constexpr auto Default = "<anonymous namespace>";

        auto range = cvtr.toLocalRange(toLiteralRange(decl->getSourceRange()), src);
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
        auto range = cvtr.toLocalRange(toLiteralRange(decl->getSourceRange()), src);
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
            auto range = cvtr.toLocalRange(toLiteralRange(etor->getSourceRange()), src);
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

        auto range = cvtr.toLocalRange(toLiteralRange(decl->getSourceRange()), src);
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
        auto range = cvtr.toLocalRange(toLiteralRange(decl->getSourceRange()), src);
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

    bool TraverseFunctionDecl(clang::FunctionDecl* decl) {
        if(!decl)
            return true;

        if(auto spec = decl->getTemplateSpecializationInfo();
           spec && !spec->isExplicitSpecialization()) {
            return true;
        }

        return Base::TraverseFunctionDecl(decl);
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        auto range = cvtr.toLocalRange(toLiteralRange(decl->getSourceRange()), src);
        DocumentSymbol symbol{
            .kind = proto::SymbolKind::Function,
            .name = decl->getNameAsString(),
            .range = range,
            .selectionRange = range,
        };
        symbol.detail = composeFuncSignature(decl);

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol), decl->getBeginLoc());
        return true;
    }

    bool TraverseParmVarDecl(clang::ParmVarDecl* decl) {
        return true;
    }

    bool TraverseVarDecl(clang::VarDecl* decl) {
        if(!decl || decl->isLocalVarDeclOrParm() || decl->isTemplated())
            return true;

        return Base::TraverseVarDecl(decl);
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        auto range = cvtr.toLocalRange(toLiteralRange(decl->getSourceRange()), src);
        DocumentSymbol symbol{
            .kind = decl->isConstexpr() ? proto::SymbolKind::Constant : proto::SymbolKind::Variable,
            .name = decl->getNameAsString(),
            .detail = decl->getType().getAsString(),
            .range = range,
            .selectionRange = range,
        };

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol), decl->getBeginLoc());
        return true;
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
index::Shared<Result> documentSymbol(ASTInfo& info, const SourceConverter& SC) {
    DocumentSymbolCollector collector{
        .src = info.srcMgr(),
        .cvtr = SC,
        .result = DocumentSymbolCollector::Storage{},
        .onlyMain = false,
    };

    collector.TraverseTranslationUnitDecl(info.tu());
    return std::move(collector.result);
}

Result documentSymbolInMainFile(ASTInfo& info, const SourceConverter& SC) {
    DocumentSymbolCollector collector{
        .src = info.srcMgr(),
        .cvtr = SC,
        .result = DocumentSymbolCollector::Storage{},
        .onlyMain = true,
        .mainID = info.srcMgr().getMainFileID(),
    };

    collector.TraverseTranslationUnitDecl(info.tu());
    return std::move(collector.result[collector.mainID]);
}

void toLspType(proto::DocumentSymbol& lspRes, const DocumentSymbol& result,
               const SourceConverter& SC, llvm::StringRef content) {
    lspRes.name = result.name;
    lspRes.detail = result.detail;
    lspRes.kind = result.kind;
    lspRes.tags = result.tags;

    lspRes.range = SC.toRange(result.range, content);
    lspRes.selectionRange = SC.toRange(result.selectionRange, content);

    lspRes.children.reserve(result.children.size());
    for(auto& child: result.children) {
        proto::DocumentSymbol lsp;
        toLspType(lsp, child, SC, content);
        lspRes.children.push_back(std::move(lsp));
    }
    lspRes.children.shrink_to_fit();
}

proto::DocumentSymbolResult toLspResult(llvm::ArrayRef<DocumentSymbol> result,
                                        llvm::StringRef content, const SourceConverter& SC) {
    proto::DocumentSymbolResult lspRes;
    lspRes.reserve(result.size());

    for(auto& symbol: result) {
        proto::DocumentSymbol lspSymbol;
        toLspType(lspSymbol, symbol, SC, content);
        lspRes.push_back(std::move(lspSymbol));
    }

    lspRes.shrink_to_fit();
    return lspRes;
}

}  // namespace feature::document_symbol

}  // namespace clice
