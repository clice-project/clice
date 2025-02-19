#include "AST/FilterASTVisitor.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/FoldingRange.h"

/// Clangd's FoldingRange Implementation:
/// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SemanticSelection.cpp

namespace clice {

namespace {

struct FoldingRangeCollector : public FilteredASTVisitor<FoldingRangeCollector> {

    using Base = FilteredASTVisitor<FoldingRangeCollector>;

    using Storage = index::Shared<feature::foldingrange::Result>;

    /// The result of folding ranges.
    Storage result;

    constexpr static auto LastColOfLine = std::numeric_limits<unsigned>::max();

    FoldingRangeCollector(ASTInfo& AST,
                          bool interestedOnly,
                          std::optional<LocalSourceRange> targetRange) :
        FilteredASTVisitor(AST, interestedOnly, targetRange), result() {}

    /// Get last column of previous line of a location.
    clang::SourceLocation prevLineLastColOf(clang::SourceLocation loc) {
        const auto& SM = AST.srcMgr();
        auto prevLine = SM.getPresumedLineNumber(loc) - 1;
        return SM.translateLineCol(AST.getInterestedFile(), prevLine, LastColOfLine);
    }

    /// Collect source range as a folding range.
    void collect(const clang::SourceRange range,
                 proto::FoldingRangeKind kind = proto::FoldingRangeKind::Region) {

        const auto& SM = AST.srcMgr();
        auto startLine = SM.getPresumedLineNumber(range.getBegin()) - 1;
        auto endLine = SM.getPresumedLineNumber(range.getEnd()) - 1;

        // Skip ranges on a single line.
        if(startLine >= endLine)
            return;

        auto fileID = interestedOnly ? AST.getInterestedFile() : SM.getFileID(range.getBegin());
        result[fileID].push_back({
            .range = AST.toLocalRange(range).second,
            .kind = kind,
        });
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        auto tokens = AST.tokBuf().expandedTokens(decl->getSourceRange());

        // Find first '{' in namespace declaration.
        auto shrink = tokens.drop_until([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() == clang::tok::l_brace;
        });

        collect({shrink.front().endLocation(), prevLineLastColOf(decl->getRBraceLoc())});
        return true;
    }

    /// Collect lambda capture list "[ ... ]".
    void collectLambdaCapture(const clang::CXXRecordDecl* decl) {
        auto tokens = AST.tokBuf().expandedTokens(decl->getSourceRange());

        auto shrink = tokens.drop_until([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() == clang::tok::TokenKind::l_square;
        });

        auto& leftSquare = shrink.front();

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

        auto& rightSquare = shrink.front();

        collect({leftSquare.endLocation(), rightSquare.location()});
    }

    /// Collect public/protected/private blocks for a non-lambda struct/class.
    void collectAccCtrlBlocks(const clang::CXXRecordDecl* decl) {
        constexpr static auto isAcccessSpecifier = [](const clang::syntax::Token& tk) -> bool {
            switch(tk.kind()) {
                case clang::tok::kw_public:
                case clang::tok::kw_protected:
                case clang::tok::kw_private: return true;
                default: return false;
            }
        };

        auto tryCollectRegion = [this](clang::SourceLocation lhs, clang::SourceLocation rhs) {
            const auto& SM = AST.srcMgr();
            // Skip continous access control keywords.
            if(SM.getPresumedLineNumber(lhs) == SM.getPresumedLineNumber(rhs))
                return;
            collect({lhs, prevLineLastColOf(rhs)});
        };

        auto tokens = AST.tokBuf().expandedTokens(decl->getSourceRange());

        // If there is no access control blocks, return.
        tokens = tokens.drop_until(isAcccessSpecifier);
        if(tokens.empty())
            return;

        auto [_, endBrace] = decl->getBraceRange();
        tokens = tokens.drop_front();  // Move to ':' after private/public/protected
        clang::SourceLocation last = tokens.front().endLocation();
        while(true) {
            tokens = tokens.drop_until(isAcccessSpecifier);
            if(tokens.empty()) {
                tryCollectRegion(last, endBrace);
                break;
            }

            tryCollectRegion(last, tokens.front().location());
            tokens = tokens.drop_front();  // Move to ':' after private/public/protected
            last = tokens.front().endLocation();
        }
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        auto [leftBrace, rightBrace] = decl->getBraceRange();
        auto name = decl->getName();
        collect({leftBrace.getLocWithOffset(1), prevLineLastColOf(rightBrace)});

        if(auto cxd = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
           cxd != nullptr && cxd->hasDefinition()) {
            collectAccCtrlBlocks(cxd);
        }
        return true;
    }

    /// Collect function parameter list between '(' and ')'.
    void collectParameterList(clang::SourceLocation leftSide, clang::SourceLocation rightSide) {
        auto tokens = AST.tokBuf().expandedTokens({leftSide, rightSide});
        auto leftParen = tokens.drop_until([](const auto& tk) {  //
            return tk.kind() == clang::tok::l_paren;
        });

        if(leftParen.empty())
            return;

        auto rightParenIter =
            std::find_if(leftParen.rbegin(), leftParen.rend(), [](const auto& tk) {
                return tk.kind() == clang::tok::r_paren;
            });

        if(rightParenIter == leftParen.rend())
            return;

        collect({leftParen.front().endLocation(), rightParenIter->location()});
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        auto leftParen = decl->getBeginLoc();
        auto rightParen = decl->hasBody()  //
                              ? decl->getBody()->getBeginLoc()
                              : decl->getSourceRange().getEnd();
        collectParameterList(leftParen, rightParen);

        // Function body was collected by `VisitCompoundStmt`.
        return true;
    }

    bool VisitLambdaExpr(const clang::LambdaExpr* expr) {
        auto [leftSquare, rightSquare] = expr->getIntroducerRange();
        collect({leftSquare.getLocWithOffset(1), prevLineLastColOf(rightSquare)});

        if(expr->hasExplicitParameters())
            collectParameterList(rightSquare, expr->getCompoundStmtBody()->getLBracLoc());

        return true;
    }

    bool VisitCompoundStmt(const clang::CompoundStmt* stmt) {
        collect({stmt->getLBracLoc(), stmt->getRBracLoc()});
        return true;
    }

    bool VisitCallExpr(const clang::CallExpr* call) {
        auto tokens = AST.tokBuf().expandedTokens(call->getSourceRange());
        if(tokens.back().kind() != clang::tok::r_paren)
            return true;

        auto rightParen = tokens.back().location();
        size_t depth = 0;
        while(!tokens.empty()) {
            auto kind = tokens.back().kind();
            if(kind == clang::tok::r_paren)
                depth += 1;
            else if(kind == clang::tok::l_paren && --depth == 0) {
                collect({tokens.back().endLocation(), rightParen});
                break;
            }
            tokens = tokens.drop_back();
        }

        return true;
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* stmt) {
        if(auto range = stmt->getParenOrBraceRange(); range.isValid())
            collect({range.getBegin().getLocWithOffset(1), range.getEnd()});

        return true;
    }

    bool VisitInitListExpr(const clang::InitListExpr* expr) {
        collect({expr->getLBraceLoc(), expr->getRBraceLoc()});
        return true;
    }

    using ASTDirectives = std::remove_reference_t<decltype(std::declval<ASTInfo>().directives())>;

    void collectDrectives(const ASTDirectives& direcs) {
        for(auto& [fileid, dirc]: direcs) {
            if(fileid != AST.getInterestedFile())
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
        const auto& SM = AST.srcMgr();

        auto lastLocOfLine = [this, &SM](clang::SourceLocation loc) {
            auto line = SM.getPresumedLineNumber(loc);
            return SM.translateLineCol(SM.getMainFileID(), line, LastColOfLine);
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
            auto eof = SM.getLocForEndOfFile(SM.getMainFileID());
            while(!stack.empty()) {
                auto last = stack.pop_back_val();
                collect({lastLocOfLine(last->loc), eof});
            }
        }
    }

    static Storage collect(ASTInfo& AST,
                           bool interestedOnly,
                           std::optional<LocalSourceRange> targetRange) {
        FoldingRangeCollector collector(AST, interestedOnly, targetRange);
        collector.collectDrectives(AST.directives());
        collector.TraverseTranslationUnitDecl(AST.tu());
        return std::move(collector.result);
    }
};

}  // namespace

namespace feature::foldingrange {

json::Value capability(json::Value clientCapabilities) {
    // Always return empty object.
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_foldingRange
    return {};
}

index::Shared<Result> foldingRange(ASTInfo& info) {
    return FoldingRangeCollector::collect(info, /*interestedOnly=*/false, std::nullopt);
}

Result foldingRange(proto::FoldingRangeParams _, ASTInfo& AST) {
    auto ranges = FoldingRangeCollector::collect(AST, /*interestedOnly=*/true, std::nullopt);
    return std::move(ranges[AST.getInterestedFile()]);
}

proto::FoldingRange toLspType(const FoldingRange& folding,
                              const SourceConverter& SC,
                              llvm::StringRef content) {
    auto range = SC.toRange(folding.range, content);
    return {
        .startLine = range.start.line,
        .endLine = range.end.line,
        .startCharacter = range.start.character,
        .endCharacter = range.end.character,
        .kind = folding.kind,
        .collapsedText = "",
    };
}

proto::FoldingRangeResult toLspResult(llvm::ArrayRef<FoldingRange> foldings,
                                      const SourceConverter& SC,
                                      llvm::StringRef content) {

    proto::FoldingRangeResult result;
    result.reserve(foldings.size());
    for(const auto& folding: foldings) {
        result.push_back(toLspType(folding, SC, content));
    }
    return result;
}

}  // namespace feature::foldingrange

}  // namespace clice
