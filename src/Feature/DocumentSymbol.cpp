#include "Feature/DocumentSymbol.h"
#include "clang/Basic/SourceLocation.h"

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

/// Use DFS to traverse the AST and collect document symbols.
struct DocumentSymbolCollector :
    public clang::RecursiveASTVisitor<DocumentSymbolCollector>,
    public LspProtoAdaptor {

    using Base = clang::RecursiveASTVisitor<DocumentSymbolCollector>;

    /// Token buffer of given AST.
    // clang::syntax::TokenBuffer* tkbuf;

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
            stack.push_back(std::move(symbol));
        else
            stack.back().children.push_back(std::move(symbol));
    }

    bool TraverseDecl(clang::Decl* decl) {
        if(!decl)
            return true;

        if(llvm::isa<clang::NamedDecl>(decl)) {
            auto* named = llvm::dyn_cast<clang::NamedDecl>(decl);
            if(notInMainFile(named->getLocation()))
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
