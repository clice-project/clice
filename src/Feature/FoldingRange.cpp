#include "Feature/FoldingRange.h"
#include "Compiler/Compilation.h"
#include "Index/Shared.h"
#include "clang/AST/RecursiveASTVisitor.h"

/// Clangd's FoldingRange Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SemanticSelection.cpp

namespace clice {

namespace {

struct FoldingRangeCollector : public clang::RecursiveASTVisitor<FoldingRangeCollector> {

    using Base = clang::RecursiveASTVisitor<FoldingRangeCollector>;
    using Storage = index::Shared<feature::folding_range::Result>;

    /// The converter used to adapt LSP protocol.
    const SourceConverter& cvtr;

    /// The source manager of given AST.
    const clang::SourceManager& src;

    /// Token buffer of given AST.
    const clang::syntax::TokenBuffer& tkbuf;

    /// The result of folding ranges.
    Storage result;

    /// True if only main file is involved.
    const bool onlyMain;

    const clang::FileID mainFileID;

    /// Do not produce folding ranges if either range ends is not within the main file.
    bool needFilter(clang::SourceLocation loc) {
        return loc.isInvalid() || (onlyMain && !src.isInMainFile(loc));
    }

    /// Get last column of previous line of a location.
    clang::SourceLocation prevLineLastColOf(clang::SourceLocation loc) {
        return src.translateLineCol(src.getMainFileID(),
                                    src.getPresumedLineNumber(loc) - 1,
                                    std::numeric_limits<unsigned>::max());
    }

    /// Collect source range as a folding range.
    void collect(const clang::SourceRange sr,
                 proto::FoldingRangeKind kind = proto::FoldingRangeKind::Region) {
        auto startLine = src.getPresumedLineNumber(sr.getBegin()) - 1;
        auto endLine = src.getPresumedLineNumber(sr.getEnd()) - 1;

        // Skip ranges on a single line.
        if(startLine >= endLine)
            return;

        auto& state = onlyMain ? result[mainFileID] : result[src.getFileID(sr.getBegin())];
        state.push_back({
            .range = cvtr.toLocalRange(sr, src),
            .kind = kind,
        });
    }

    bool TraverseNamespaceDecl(clang::NamespaceDecl* decl) {
        if(!decl || needFilter(decl->getLocation()))
            return true;

        return Base::TraverseNamespaceDecl(decl);
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        auto tks = tkbuf.expandedTokens(decl->getSourceRange());

        // Find first '{' in namespace declaration.
        auto shrink = tks.drop_until([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() == clang::tok::l_brace;
        });

        collect({shrink.front().endLocation(), prevLineLastColOf(shrink.back().location())});
        return true;
    }

    /// Collect lambda capture list "[ ... ]".
    void collectLambdaCapture(const clang::CXXRecordDecl* decl) {
        auto tks = tkbuf.expandedTokens(decl->getSourceRange());

        auto shrink = tks.drop_until([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() == clang::tok::TokenKind::l_square;
        });

        auto ls = shrink.front();
        {
            shrink = shrink.drop_front();
            shrink = shrink.drop_until([depth = 0](const clang::syntax::Token& tk) mutable {
                switch(tk.kind()) {
                    case clang::tok::TokenKind::r_square: {
                        if(depth-- == 0)
                            return true;
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

        auto tks = tkbuf.expandedTokens(decl->getSourceRange());

        auto tryCollectRegion = [this](clang::SourceLocation ll, clang::SourceLocation lr) {
            // Skip continous access control keywords.
            if(src.getPresumedLineNumber(ll) == src.getPresumedLineNumber(lr))
                return;
            collect({ll, prevLineLastColOf(lr)});
        };

        // If there is no access control blocks, return.
        tks = tks.drop_until(is_accctrl);
        if(tks.empty())
            return;

        auto [_, rb] = decl->getBraceRange();
        tks = tks.drop_front();  // Move to ':' after private/public/protected
        clang::SourceLocation last = tks.front().endLocation();
        while(true) {
            tks = tks.drop_until(is_accctrl);
            if(tks.empty()) {
                tryCollectRegion(last, rb);
                break;
            }

            tryCollectRegion(last, tks.front().location());
            tks = tks.drop_front();  // Move to ':' after private/public/protected
            last = tks.front().endLocation();
        }
    }

    bool TraverseDecl(clang ::Decl* decl) {
        if(!decl || needFilter(decl->getLocation()))
            return true;

        return Base::TraverseDecl(decl);
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        auto [lb, rb] = decl->getBraceRange();
        auto name = decl->getName();
        collect({lb.getLocWithOffset(1), prevLineLastColOf(rb)});

        if(auto cxd = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
           cxd != nullptr && cxd->hasDefinition()) {
            collectAccCtrlBlocks(cxd);
        }
        return true;
    }

    /// Collect function parameter list between '(' and ')'.
    void collectParameterList(clang::SourceLocation left, clang::SourceLocation right) {
        auto tks = tkbuf.expandedTokens({left, right});

        tks = tks.drop_until([](const auto& tk) { return tk.kind() == clang::tok::l_paren; });
        if(tks.empty())
            return;

        auto iter = std::find_if(tks.rbegin(), tks.rend(), [](const auto& tk) {
            return tk.kind() == clang::tok::r_paren;
        });

        if(iter == tks.rend())
            return;

        auto lr = tks.front().endLocation();
        auto rr = iter->location();
        collect({lr, prevLineLastColOf(rr)});
    }

    bool TraverseFunctionDecl(clang ::FunctionDecl* decl) {
        if(!decl || needFilter(decl->getLocation()))
            return true;

        return Base::TraverseFunctionDecl(decl);
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        // Left parent.
        auto pl = decl->isTemplateDecl()
                      ? decl->getTemplateParameterList(1)->getSourceRange().getEnd()
                      : decl->getBeginLoc();

        // Right parent.
        auto pr =
            decl->hasBody() ? decl->getBody()->getBeginLoc() : decl->getSourceRange().getEnd();

        collectParameterList(pl, pr);

        // Function body was collected by `VisitCompoundStmt`.
        return true;
    }

    bool TraverseLambdaExpr(clang ::LambdaExpr* expr) {
        if(!expr || needFilter(expr->getBeginLoc()))
            return true;

        return Base::TraverseLambdaExpr(expr);
    }

    bool VisitLambdaExpr(const clang::LambdaExpr* expr) {
        auto [il, ir] = expr->getIntroducerRange();
        collect({il.getLocWithOffset(1), prevLineLastColOf(ir)});

        if(expr->hasExplicitParameters())
            collectParameterList(ir, expr->getCompoundStmtBody()->getLBracLoc());

        return true;
    }

    bool TraverseCompoundStmt(clang ::CompoundStmt* stmt) {
        if(!stmt || needFilter(stmt->getBeginLoc()))
            return true;

        return Base::TraverseCompoundStmt(stmt);
    }

    bool VisitCompoundStmt(const clang::CompoundStmt* stmt) {
        collect({stmt->getLBracLoc().getLocWithOffset(1), prevLineLastColOf(stmt->getRBracLoc())});
        return true;
    }

    bool TraverseCallExpr(clang ::CallExpr* expr) {
        if(!expr || needFilter(expr->getBeginLoc()))
            return true;

        return Base::TraverseCallExpr(expr);
    }

    bool VisitCallExpr(const clang::CallExpr* expr) {
        auto tks = tkbuf.expandedTokens(expr->getSourceRange());
        if(tks.back().kind() != clang::tok::r_paren)
            return true;

        auto rp = tks.back().location();
        size_t depth = 0;
        while(!tks.empty()) {
            auto kind = tks.back().kind();
            if(kind == clang::tok::r_paren)
                depth += 1;
            else if(kind == clang::tok::l_paren && --depth == 0) {
                collect({tks.back().endLocation(), prevLineLastColOf(rp)});
                break;
            }
            tks = tks.drop_back();
        }

        return true;
    }

    bool TraverseCXXConstructExpr(clang::CXXConstructExpr* expr) {
        if(!expr || needFilter(expr->getLocation()))
            return true;

        return Base::TraverseCXXConstructExpr(expr);
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* stmt) {
        if(auto range = stmt->getParenOrBraceRange(); range.isValid())
            collect({range.getBegin().getLocWithOffset(1), prevLineLastColOf(range.getEnd())});
        return true;
    }

    bool TraverseInitListExpr(clang::InitListExpr* expr) {
        if(!expr || needFilter(expr->getBeginLoc()))
            return true;

        return Base::TraverseInitListExpr(expr);
    }

    bool VisitInitListExpr(const clang::InitListExpr* expr) {
        collect({
            expr->getLBraceLoc().getLocWithOffset(1),
            prevLineLastColOf(expr->getRBraceLoc()),
        });
        return true;
    }

    using ASTDirectives = std::remove_reference_t<decltype(std::declval<ASTInfo>().directives())>;

    void collectDrectives(const ASTDirectives& direcs) {
        for(auto& [fileid, dirc]: direcs) {
            if(fileid != src.getMainFileID())
                continue;

            collectConditionMacro(dirc.conditions);
            collectPragmaRegion(dirc.pragmas);

            /// TODO:
            /// Collect multiline include statement.
        }
    }

    /// Collect all condition macro's block as folding range.
    void collectConditionMacro(const std::vector<Condition>& conds) {

        // All condition directives have been stored in `conds` variable, ordered by presumed line
        // number increasement, so use a stack to handle the branch structure.
        llvm::SmallVector<const Condition*> stack = {};

        for(auto& cond: conds) {
            switch(cond.kind) {
                case Condition::BranchKind::If:
                case Condition::BranchKind::Ifdef:
                case Condition::BranchKind::Ifndef:
                case Condition::BranchKind::Elif:
                case Condition::BranchKind::Elifndef: {
                    stack.push_back(&cond);
                    break;
                }

                case Condition::BranchKind::Else: {
                    if(!stack.empty()) {
                        auto last = stack.pop_back_val();
                        collect({last->loc, prevLineLastColOf(cond.loc)});
                    }

                    stack.push_back(&cond);
                    break;
                }

                case Condition::BranchKind::EndIf: {
                    if(!stack.empty()) {
                        auto last = stack.pop_back_val();
                        collect({last->loc, prevLineLastColOf(cond.loc)});
                    }
                    break;
                }

                default: break;
            }
        }
    }

    /// Collect all condition macro's block as folding range.
    void collectPragmaRegion(const std::vector<Pragma>& pragmas) {
        auto lastLocOfLine = [this](clang::SourceLocation loc) {
            return src.translateLineCol(src.getMainFileID(),
                                        src.getPresumedLineNumber(loc),
                                        std::numeric_limits<unsigned>::max());
        };

        llvm::SmallVector<const Pragma*> stack = {};
        for(auto& pragma: pragmas) {
            switch(pragma.kind) {
                case Pragma::Kind::Region: stack.push_back(&pragma); break;
                case Pragma::Kind::EndRegion:
                    if(!stack.empty()) {
                        auto last = stack.pop_back_val();
                        collect({lastLocOfLine(last->loc), prevLineLastColOf(pragma.loc)});
                    }
                    break;
                default: break;
            }
        }

        // If there is some region without end pragma, use the end of file as the end region.
        if(!stack.empty()) {
            auto eof = src.getLocForEndOfFile(src.getMainFileID());
            while(!stack.empty()) {
                auto last = stack.pop_back_val();
                collect({lastLocOfLine(last->loc), eof});
            }
        }
    }
};

}  // namespace

namespace feature::folding_range {

json::Value capability(json::Value clientCapabilities) {
    // Always return empty object.
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_foldingRange
    return {};
}

index::Shared<Result> foldingRange(ASTInfo& info, const SourceConverter& converter) {
    FoldingRangeCollector collector{
        .cvtr = converter,
        .src = info.srcMgr(),
        .tkbuf = info.tokBuf(),
        .result = FoldingRangeCollector::Storage{},
        .onlyMain = false,
    };

    collector.collectDrectives(info.directives());
    collector.TraverseTranslationUnitDecl(info.tu());

    return std::move(collector.result);
}

Result foldingRange(FoldingRangeParams& _, ASTInfo& info, const SourceConverter& converter) {
    FoldingRangeCollector collector{
        .cvtr = converter,
        .src = info.srcMgr(),
        .tkbuf = info.tokBuf(),
        .result = FoldingRangeCollector::Storage{},
        .onlyMain = true,
        .mainFileID = info.srcMgr().getMainFileID(),
    };
    collector.result.reserve(1);

    collector.collectDrectives(info.directives());
    collector.TraverseTranslationUnitDecl(info.tu());

    return std::move(collector.result[collector.mainFileID]);
}

proto::FoldingRangeResult toLspResult(llvm::ArrayRef<FoldingRange> ranges, llvm::StringRef content,
                                      const SourceConverter& SC) {
    proto::FoldingRangeResult results;
    results.reserve(ranges.size());

    for(auto& range: ranges) {
        auto lspRange = SC.toRange(range.range, content);
        results.push_back({
            .startLine = lspRange.start.line,
            .endLine = lspRange.end.line,
            .startCharacter = lspRange.start.character,
            .endCharacter = lspRange.end.character,
            .kind = range.kind,
        });
    }

    results.shrink_to_fit();
    return results;
}

}  // namespace feature::folding_range

}  // namespace clice
