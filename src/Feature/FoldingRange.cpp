#include "Feature/FoldingRange.h"
#include "Compiler/Compiler.h"

/// Clangd's FoldingRange Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SemanticSelection.cpp

namespace clice {

namespace {

struct FoldingRangeCollector : public clang::RecursiveASTVisitor<FoldingRangeCollector> {

    using Base = clang::RecursiveASTVisitor<FoldingRangeCollector>;

    /// The source manager of given AST.
    clang::SourceManager* src;

    /// Token buffer of given AST.
    clang::syntax::TokenBuffer* tkbuf;

    /// The result of folding ranges.
    proto::FoldingRangeResult result;

    /// Do not produce folding ranges if either range ends is not within the main file.
    bool needFilter(clang::SourceLocation loc) {
        return loc.isInvalid() || !src->isInMainFile(loc);
    }

    /// Get last column of previous line of a location.
    clang::SourceLocation prevLineLastColOf(clang::SourceLocation loc) {
        return src->translateLineCol(src->getMainFileID(),
                                     src->getPresumedLineNumber(loc) - 1,
                                     std::numeric_limits<unsigned>::max());
    }

    /// Collect source range as a folding range.
    void collect(const clang::SourceRange sr) {
        // - 1: convert 1-1 based LSP location to 0-0 based LSP location.
        proto::FoldingRange Range;
        Range.startLine = src->getPresumedLineNumber(sr.getBegin()) - 1;
        Range.endLine = src->getPresumedLineNumber(sr.getEnd()) - 1;

        // Skip ranges on a single line.
        if(Range.startLine >= Range.endLine)
            return;

        Range.startCharacter = src->getPresumedColumnNumber(sr.getBegin()) - 1;
        Range.endCharacter = src->getPresumedColumnNumber(sr.getEnd()) - 1;
        result.push_back(Range);
    }

    TRAVERSE_DECL(NamespaceDecl) {
        if(!decl || needFilter(decl->getLocation()))
            return true;

        return Base::TraverseNamespaceDecl(decl);
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        auto tks = tkbuf->expandedTokens(decl->getSourceRange());

        // Find first '{'.
        auto shrink = tks.drop_while([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() != clang::tok::l_brace;
        });

        collect({shrink.front().endLocation(), prevLineLastColOf(shrink.back().location())});
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
                    case clang::tok::TokenKind::r_square: {
                        if(depth == 0)
                            return true;
                        depth--;
                        break;
                    }

                    case clang::tok::TokenKind::l_square: depth++; break;

                    default: break;
                }
                return false;
            });
        }
        auto rs = shrink.front();

        collect({ls.location(), rs.location()});
    }

    /// Collect public/protected/private blocks for a non-lambda struct/class.
    void collectAccCtrlBlocks(const clang::CXXRecordDecl* decl) {
        constexpr static auto is_accctrl = [](const clang::syntax::Token& tk) -> bool {
            switch(tk.kind()) {
                case clang::tok::kw_public:
                case clang::tok::kw_protected:
                case clang::tok::kw_private: return true;
                default: return false;
            }
        };

        auto tks = tkbuf->expandedTokens(decl->getSourceRange());

        // If there is no access control blocks, return.
        if(!std::ranges::any_of(tks, is_accctrl)) {
            return;
        }

        // Move to first token after '{'
        auto [lb, rb] = decl->getBraceRange();
        tks = tks.drop_while([&lb](auto& tk) { return tk.location() <= lb; });

        // -1:  to avoid empty region like
        // public:
        // private:
        auto tryCollectRegion = [this](clang::SourceLocation ll, clang::SourceLocation lr) {
            collect({ll, prevLineLastColOf(lr)});
        };

        clang::SourceLocation last = lb;
        while(last <= rb) {
            tks = tks.drop_until([](auto& tk) { return is_accctrl(tk); });
            if(tks.empty()) {
                tryCollectRegion(last, rb);
                break;
            }

            last = tks.front().endLocation();
            tryCollectRegion(last, tks.front().location());
            tks = tks.drop_front();
        }
    }

    TRAVERSE_DECL(Decl) {
        if(!decl || needFilter(decl->getLocation()))
            return true;

        return Base::TraverseDecl(decl);
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        auto [lb, rb] = decl->getBraceRange();
        collect({lb.getLocWithOffset(1), prevLineLastColOf(rb)});

        if(auto cxd = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
           cxd != nullptr && cxd->hasDefinition()) {
            collectAccCtrlBlocks(cxd);
        }
        return true;
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        /// TODO: fold parameter list

        // {
        //     auto [lb, rb] = decl->getParametersSourceRange();
        //     collect({lb.getLocWithOffset(1), prevLineLastColOf(rb)});
        // }

        if(!decl->hasBody())
            return true;

        auto [lb, rb] = decl->getBody()->getSourceRange();
        collect({lb.getLocWithOffset(1), prevLineLastColOf(rb)});
        return true;
    }

    using ASTDirectives = std::remove_reference_t<decltype(std::declval<ASTInfo>().directives())>;

    void collectDrectives(const ASTDirectives& directives) {
        for(auto& [fileid, dirc]: directives) {
            if(fileid != src->getMainFileID())
                continue;

            for(auto& cond: dirc.conditions) {
                collect(cond.conditionRange);
            }
        }
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

    collector.collectDrectives(ast.directives());
    collector.TraverseTranslationUnitDecl(ast.tu());

    return std::move(collector.result);
}

}  // namespace feature

}  // namespace clice
