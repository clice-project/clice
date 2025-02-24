#include "AST/FilterASTVisitor.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/FoldingRange.h"

namespace clice::feature {

namespace {

class FoldingRangeCollector : public FilteredASTVisitor<FoldingRangeCollector> {
public:
    FoldingRangeCollector(ASTInfo& AST, bool interestedOnly) :
        FilteredASTVisitor(AST, interestedOnly, std::nullopt), result() {}

    /// Cache extra line number as the inner storage to speedup the collection.
    struct RichFolding : FoldingRange {
        uint32_t startLine;
        uint32_t endLine;
    };

    constexpr static auto LastColOfLine = std::numeric_limits<unsigned>::max();

    /// Collect source range as a folding range.
    void collect(clang::SourceRange range,
                 std::pair<int, int> offsetFix = {0, 0},
                 FoldingRangeKind kind = FoldingRangeKind::Region) {

        const auto& SM = AST.srcMgr();
        unsigned startLine = SM.getPresumedLineNumber(range.getBegin()) - 1;
        unsigned endLine = SM.getPresumedLineNumber(range.getEnd()) - 1;

        // Skip ranges on a single line.
        if(startLine >= endLine)
            return;

        auto fileID = interestedOnly ? AST.getInterestedFile() : SM.getFileID(range.getBegin());
        auto& state = result[fileID];

        if(auto beg = range.getBegin(); beg.isMacroID()) {
            auto cursor =
                SM.translateLineCol(fileID, SM.getExpansionLineNumber(beg), LastColOfLine);
            range.setBegin(cursor);
            offsetFix.first = 0;
        }
        if(auto end = range.getEnd(); end.isMacroID()) {
            range.setEnd(SM.translateLineCol(fileID,
                                             SM.getExpansionLineNumber(end),
                                             SM.getExpansionColumnNumber(end)));
            offsetFix.second = 0;
        }

        assert(range.isValid());

        auto [leftLocal, rightLocal] = AST.toLocalRange(range).second;
        LocalSourceRange fixed{leftLocal + offsetFix.first, rightLocal + offsetFix.second};
        if(state.empty() || state.back().startLine != startLine) {
            state.push_back({kind, fixed, startLine, endLine});
        }
    }

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        auto tokens = AST.tokBuf().expandedTokens(decl->getSourceRange());

        // Find first '{' in namespace declaration.
        auto shrink = tokens.drop_until([](const clang::syntax::Token& tk) -> bool {
            return tk.kind() == clang::tok::l_brace;
        });

        collect({shrink.front().location(), decl->getRBraceLoc()}, {1, -1});
        return true;
    }

    /// Collect public/protected/private blocks for a non-lambda struct/class.
    void collectAccessSpecDecls(const clang::RecordDecl* RD) {
        const clang::AccessSpecDecl* lastAccess = nullptr;

        for(auto* decl: RD->decls()) {
            if(auto* AS = llvm::dyn_cast<clang::AccessSpecDecl>(decl)) {
                if(lastAccess) {
                    auto spec = AS->getAccessUnsafe();
                    int offsetToSpecStart = spec == clang::AS_private  ? 7
                                            : spec == clang::AS_public ? 6
                                                                       : 9;
                    collect({lastAccess->getColonLoc(), AS->getAccessSpecifierLoc()},
                            {1, -offsetToSpecStart});
                }
                lastAccess = AS;
            }
        }

        // The last access specifier block.
        if(lastAccess) {
            collect({lastAccess->getColonLoc(), RD->getBraceRange().getEnd()}, {1, -1});
        }
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        // Collect definition of class/struct/enum.
        auto [leftBrace, rightBrace] = decl->getBraceRange();
        auto name = decl->getName();
        collect({leftBrace, rightBrace}, {1, -1});

        if(auto RD = llvm::dyn_cast<clang::RecordDecl>(decl)) {
            collectAccessSpecDecls(RD);
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

        collect({leftParen.front().location(), rightParenIter->location()}, {1, -1});
    }

    void collectCompoundStmt(const clang::Stmt* stmt) {
        if(auto* CS = llvm::dyn_cast<clang::CompoundStmt>(stmt)) {
            collect({CS->getLBracLoc(), CS->getRBracLoc()}, {1, -1});
            for(auto child: stmt->children()) {
                collectCompoundStmt(child);
            }
        }
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        auto leftParen = decl->getBeginLoc();
        auto rightParen = decl->hasBody()  //
                              ? decl->getBody()->getBeginLoc()
                              : decl->getSourceRange().getEnd();
        collectParameterList(leftParen, rightParen);

        if(decl->hasBody()) {
            auto [leftBrace, rightBrace] = decl->getBody()->getSourceRange();
            collect({leftBrace, rightBrace}, {1, -1});
            collectCompoundStmt(decl->getBody());
        }
        return true;
    }

    bool VisitLambdaExpr(const clang::LambdaExpr* lambda) {
        auto introduceRange = lambda->getIntroducerRange();
        assert(introduceRange.isValid() && "Invalid introduce range.");
        collect(introduceRange, {1, -1});

        if(lambda->hasExplicitParameters()) {
            collectParameterList(introduceRange.getEnd(),
                                 lambda->getCompoundStmtBody()->getBeginLoc());
        }

        collectCompoundStmt(lambda->getBody());
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
                collect({tokens.back().location(), rightParen}, {1, -1});
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
        collect({expr->getLBraceLoc(), expr->getRBraceLoc()}, {1, -1});
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
                        collect({last->conditionRange.getEnd(), cond.loc}, {0, -1});
                    }

                    stack.push_back(&cond);
                    break;
                }

                case Condition::BranchKind::EndIf: {
                    if(!stack.empty()) {
                        auto last = stack.pop_back_val();

                        // For a directive without condition range e.g #else
                        // its condition range is invalid.
                        if(last->conditionRange.isValid()) {
                            collect({last->conditionRange.getBegin(), cond.loc}, {0, -1});
                        } else {
                            collect({last->loc, cond.loc},
                                    {refl::enum_name(cond.kind).length(), -1});
                        }
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
                        collect({lastLocOfLine(last->loc), pragma.loc}, {0, -1});
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

    static index::Shared<std::vector<FoldingRange>> extract(const Storage& storage) {
        llvm::DenseMap<clang::FileID, std::vector<FoldingRange>> extracted;
        for(auto& [fileID, richs]: storage) {
            std::vector<FoldingRange> res;
            res.reserve(richs.size());
            for(auto& rich: richs) {
                res.push_back(rich);
            }
            extracted[fileID] = std::move(res);
        }
        return extracted;
    }

    static index::Shared<std::vector<FoldingRange>> collect(ASTInfo& AST, bool interestedOnly) {
        FoldingRangeCollector collector(AST, interestedOnly);
        collector.collectDrectives(AST.directives());
        collector.TraverseTranslationUnitDecl(AST.tu());
        return extract(collector.result);
    }

private:
    index::Shared<std::vector<RichFolding>> result;
};

}  // namespace

std::vector<FoldingRange> foldingRange(ASTInfo& AST) {
    auto ranges = FoldingRangeCollector::collect(AST, /*interestedOnly=*/true);
    return std::move(ranges[AST.getInterestedFile()]);
}

index::Shared<std::vector<FoldingRange>> indexFoldingRange(ASTInfo& AST) {
    return FoldingRangeCollector::collect(AST, /*interestedOnly=*/false);

}  // namespace feature

}  // namespace clice::feature
