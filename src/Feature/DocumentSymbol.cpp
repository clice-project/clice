#include "Feature/DocumentSymbol.h"

namespace clice {

namespace {

struct LspProtoAdaptor {

    clang::SourceManager* src;

    bool isInMainFile(clang::SourceLocation loc) {
        return loc.isValid() && src->isInMainFile(loc);
    }

    bool notInMainFile(clang::SourceLocation loc) {
        return !isInMainFile(loc);
    }

    proto::Position toLspPosition(clang::SourceLocation loc) {
        auto presumed = src->getPresumedLoc(loc);
        return {
            .line = presumed.getLine() - 1,
            .character = presumed.getColumn() - 1,
        };
    }

    proto::Range toLspRange(clang::SourceRange sr) {
        return {
            .start = toLspPosition(sr.getBegin()),
            .end = toLspPosition(sr.getEnd()),
        };
    }
};

/// Clangd's DocumentSymbol Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/FindSymbols.cpp#L286

/// Use DFS to traverse the AST and collect document symbols.
struct DocumentSymbolCollector :
    public clang::RecursiveASTVisitor<DocumentSymbolCollector>,
    public LspProtoAdaptor {

    using Base = clang::RecursiveASTVisitor<DocumentSymbolCollector>;

    /// DFS state stack.
    std::vector<proto::DocumentSymbol> stack;

    /// Result of document symbols.
    std::vector<proto::DocumentSymbol> result;

    /// Entry a new AST node which may has some children nodes.
    void entry(proto::DocumentSymbol symbol) {
        stack.push_back(std::move(symbol));
    }

    /// Leave the current AST node.
    void leave() {
        stack.back().children.shrink_to_fit();
        auto last = std::move(stack.back());
        stack.pop_back();

        if(stack.empty())
            result.push_back(std::move(last));
        else
            stack.back().children.push_back(std::move(last));
    }

    /// Collect a leaf node as the DocumentSymbol.
    void collect(proto::DocumentSymbol symbol) {
        if(stack.empty())
            result.push_back(std::move(symbol));
        else
            stack.back().children.push_back(std::move(symbol));
    }

    /// Mark the symbol as deprecated.
    void markDeprecated(proto::DocumentSymbol& symbol) {
        symbol.tags.push_back(proto::SymbolTag{proto::SymbolTag::Deprecated});
        symbol.deprecated = true;
    }

    bool TraverseDecl(clang::Decl* decl) {
        if(!decl)
            return true;

        if(llvm::isa<clang::TranslationUnitDecl>(decl))
            return Base::TraverseDecl(decl);

        if(!llvm::isa<clang::NamedDecl>(decl))
            return true;

        // auto* named = llvm::dyn_cast<clang::NamedDecl>(decl);
        // auto name = named->getName();

        if(notInMainFile(decl->getLocation()) || decl->isImplicit())
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

        auto range = toLspRange(decl->getSourceRange());
        proto::DocumentSymbol symbol{
            .name = decl->isAnonymousNamespace() ? Default : decl->getNameAsString(),
            .kind = proto::SymbolKind::Namespace,
            .range = range,
            .selectionRange = range,
        };

        entry(std::move(symbol));
        bool res = Base::TraverseNamespaceDecl(decl);
        leave();

        return res;
    }

    bool TraverseEnumDecl(clang::EnumDecl* decl) {
        auto range = toLspRange(decl->getSourceRange());
        proto::DocumentSymbol symbol{
            .name = decl->getNameAsString(),
            .kind = proto::SymbolKind::Enum,
            .range = range,
            .selectionRange = range,
        };

        entry(std::move(symbol));
        bool res = Base::TraverseEnumDecl(decl);
        leave();

        return res;
    }

    bool VisitEnumDecl(const clang::EnumDecl* decl) {
        for(auto* enumerator: decl->enumerators()) {
            auto range = toLspRange(enumerator->getSourceRange());
            proto::DocumentSymbol symbol{
                .name = enumerator->getNameAsString(),
                .kind = proto::SymbolKind::EnumMember,
                .range = range,
                .selectionRange = range,
            };

            // Show the initializer value as the detail.
            llvm::SmallString<32> sstr;
            sstr.append("= ");
            enumerator->getInitVal().toString(sstr);
            if(sstr.size() > 10)
                symbol.detail = "<initializer>";
            else
                symbol.detail = sstr.str().slice(0, sstr.size());

            if(enumerator->isDeprecated())
                markDeprecated(symbol);

            collect(std::move(symbol));
        }

        return true;
    }

    bool TraverseCXXRecordDecl(clang::CXXRecordDecl* decl) {
        constexpr auto Default = "<anonymous struct>";

        auto range = toLspRange(decl->getSourceRange());
        proto::DocumentSymbol symbol{
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

        entry(std::move(symbol));
        bool res = Base::TraverseCXXRecordDecl(decl);
        leave();
        return res;
    }

    bool VisitFieldDecl(const clang::FieldDecl* decl) {
        auto range = toLspRange(decl->getSourceRange());
        proto::DocumentSymbol symbol{
            .name = decl->getNameAsString(),
            .kind = proto::SymbolKind::Field,
            .range = range,
            .selectionRange = range,
        };
        symbol.detail = decl->getType().getAsString();

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol));
        return true;
    }

    std::string composeFuncSignature(const clang::FunctionDecl* decl) {
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

        auto name = decl->getName();
        if(auto spec = decl->getTemplateSpecializationInfo();
           spec && !spec->isExplicitSpecialization()) {
            return true;
        }

        return Base::TraverseFunctionDecl(decl);
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        auto range = toLspRange(decl->getSourceRange());
        proto::DocumentSymbol symbol{
            .name = decl->getNameAsString(),
            .kind = proto::SymbolKind::Function,
            .range = range,
            .selectionRange = range,
        };
        symbol.detail = composeFuncSignature(decl);

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol));
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
        auto range = toLspRange(decl->getSourceRange());
        proto::DocumentSymbol symbol{
            .name = decl->getNameAsString(),
            .detail = decl->getType().getAsString(),
            .kind = decl->isConstexpr() ? proto::SymbolKind::Constant : proto::SymbolKind::Variable,
            .range = range,
            .selectionRange = range,
        };

        if(decl->isDeprecated())
            markDeprecated(symbol);

        collect(std::move(symbol));
        return true;
    }
};

}  // namespace

namespace feature {

json::Value documentSymbolCapability(json::Value clientCapabilities) {
    /// TODO:
    return {};
}

proto::DocumentSymbolResult documentSymbol(proto::DocumentSymbolParams params, ASTInfo& ast) {
    DocumentSymbolCollector collector;
    collector.src = &ast.srcMgr();
    collector.TraverseDecl(ast.tu());
    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
