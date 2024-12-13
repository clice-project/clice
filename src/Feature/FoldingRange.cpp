#include "Feature/FoldingRange.h"
#include "Compiler/Compiler.h"

/// Clangd's FoldingRange Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SemanticSelection.cpp

namespace clice {

namespace {

struct FoldingRangeCollector : public clang::RecursiveASTVisitor<FoldingRangeCollector> {
    /// The source manager of given AST.
    clang::SourceManager* src;

    /// Token buffer of given AST.
    clang::syntax::TokenBuffer* tkbuf;

    /// The result of folding ranges.
    proto::FoldingRangeResult result;

    /// Collect source range as a folding range.
    void collect(const clang::SourceRange sr) {
        if(sr.isInvalid())
            return;

        proto::FoldingRange Range;
        Range.startLine = src->getPresumedLineNumber(sr.getBegin()) - 2;
        Range.endLine = src->getPresumedLineNumber(sr.getEnd()) - 2;

        // Skip ranges on a single line.
        if(Range.startLine >= Range.endLine)
            return;

        Range.startCharacter = src->getPresumedColumnNumber(sr.getBegin());
        Range.endCharacter = src->getPresumedColumnNumber(sr.getEnd());

        result.push_back(Range);
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        auto tks = tkbuf->expandedTokens(decl->getSourceRange());

        // Find first '{'.
        auto shrink = tks.drop_while([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() != clang::tok::l_brace;
        });

        collect({shrink.front().endLocation(), shrink.back().location()});
        return true;
    }

    /// Collect lambda capture list "[ ... ]".
    void collectLambdaCapture(const clang::CXXRecordDecl* decl) {
        auto tks = tkbuf->expandedTokens(decl->getSourceRange());

        auto shrink = tks.drop_while([](const clang::syntax::Token& tk) {
            return tk.kind() != clang::tok::TokenKind::l_square;
        });

        auto ls = shrink.front();
        {
            shrink = shrink.drop_front();
            shrink = shrink.drop_until([depth = 0](const clang::syntax::Token& tk) mutable {
                switch(tk.kind()) {
                    case clang::tok::TokenKind::l_square: {
                        depth++;
                        break;
                    }
                    case clang::tok::TokenKind::r_square: {
                        if(depth == 0)
                            return true;
                        depth--;
                        break;
                    }
                    default: break;
                }
                return false;
            });
        }
        auto rs = shrink.front();

        collect({ls.location(), rs.location()});
    }

    /// Collect public/protected/private blocks.
    void collectAccCtrlBlocks(const clang::CXXRecordDecl* decl) {
        auto tks = tkbuf->expandedTokens(decl->getSourceRange());
        /// TODO:
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        auto name = decl->getName();
        collect(decl->getBraceRange());

        if(auto cxd = llvm::dyn_cast<clang::CXXRecordDecl>(decl); cxd != nullptr) {
            if(!cxd->hasDefinition() || !cxd->hasDirectFields())
                return true;

            if(cxd->isLambda())
                collectLambdaCapture(cxd);
            else
                collectAccCtrlBlocks(cxd);
        }

        return true;
    }

    bool VisitFunctionDecl(clang::FunctionDecl* decl) {
        if(!decl->hasBody())
            return true;

        collect(decl->getBody()->getSourceRange());
        return true;
    }
};

}  // namespace

namespace feature {

proto::FoldingRangeResult foldingRange(FoldingRangeParams& _, ASTInfo& ast) {

    FoldingRangeCollector collector{
        .src = &ast.srcMgr(),
        .tkbuf = &ast.tokBuf(),
        .result = {},
    };

    collector.TraverseTranslationUnitDecl(ast.tu());

    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
