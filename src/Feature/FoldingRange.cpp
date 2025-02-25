#include "AST/FilterASTVisitor.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/FoldingRange.h"

namespace clice::feature {

namespace {

class FoldingRangeCollector : public FilteredASTVisitor<FoldingRangeCollector> {
public:
    FoldingRangeCollector(ASTInfo& AST, bool interestedOnly) :
        FilteredASTVisitor(AST, interestedOnly, std::nullopt), SM(AST.srcMgr()), TB(AST.tokBuf()) {}

    constexpr static auto LastColOfLine = std::numeric_limits<unsigned>::max();

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        // Find first '{' in namespace declaration.
        auto shrink = TB.expandedTokens(decl->getSourceRange())
                          .drop_until([](const clang::syntax::Token& token) -> bool {
                              return token.kind() == clang::tok::l_brace;
                          });

        /// If The AST is not complete, we may cannot find the '{'.
        if(shrink.empty()) {
            return true;
        }

        /// Collect namespace.
        clang::SourceRange range(shrink.front().location(), decl->getRBraceLoc());
        addRange(range, FoldingRangeKind::Namespace, "{...}");

        return true;
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        /// If it's a forward declaration, nothing to do.
        if(!decl->isThisDeclarationADefinition()) {
            return true;
        }

        // Collect the definition of class/struct/enum/union.
        FoldingRangeKind kind = decl->isStruct()  ? FoldingRangeKind::Struct
                                : decl->isClass() ? FoldingRangeKind::Class
                                : decl->isUnion() ? FoldingRangeKind::Union
                                                  : FoldingRangeKind::Enum;
        addRange(decl->getBraceRange(), kind, "{...}");

        /// Collect public/protected/private blocks for a non-lambda struct/class.
        if(auto RD = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
            if(RD->isLambda() || RD->isImplicit()) {
                return true;
            }

            clang::AccessSpecDecl* last = nullptr;
            for(auto* decl: RD->decls()) {
                if(auto* AS = llvm::dyn_cast<clang::AccessSpecDecl>(decl)) {
                    if(last) {
                        addRange(
                            clang::SourceRange(last->getColonLoc(), AS->getAccessSpecifierLoc()),
                            FoldingRangeKind::AccessSpecifier,
                            "");
                    }
                    last = AS;
                }
            }

            if(last) {
                addRange(clang::SourceRange(last->getColonLoc(), RD->getBraceRange().getEnd()),
                         FoldingRangeKind::AccessSpecifier,
                         "");
            }
        }

        return true;
    }

    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        /// If it's a forward declaration, try to collect the parameter list.
        if(!decl->doesThisDeclarationHaveABody()) {
            collectParameterList(decl->getSourceRange());
        } else {
            collectParameterList(decl->getBeginLoc(), decl->getBody()->getBeginLoc());

            /// Collect function body.
            addRange(decl->getBody()->getSourceRange(), FoldingRangeKind::FunctionBody, "{...}");
        }

        return true;
    }

    bool VisitLambdaExpr(const clang::LambdaExpr* lambda) {
        auto introduceRange = lambda->getIntroducerRange();
        /// Collect lambda capture list.
        addRange(lambda->getIntroducerRange(), FoldingRangeKind::LambdaCapture, "[...]");

        /// Collect explicit parameter list.
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
                addRange({tokens.back().location(), rightParen},
                         FoldingRangeKind::FunctionCall,
                         "(...)");
                break;
            }
            tokens = tokens.drop_back();
        }

        return true;
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* stmt) {
        if(auto range = stmt->getParenOrBraceRange(); range.isValid()) {
            addRange({range.getBegin().getLocWithOffset(1), range.getEnd()},
                     FoldingRangeKind::FunctionCall,
                     "(...)");
        }
        return true;
    }

    bool VisitInitListExpr(const clang::InitListExpr* expr) {
        addRange({expr->getLBraceLoc(), expr->getRBraceLoc()},
                 FoldingRangeKind::Initializer,
                 "{...}");
        return true;
    }

    auto buildForFile(ASTInfo& AST) {
        TraverseTranslationUnitDecl(AST.tu());
        collectDrectives(AST.directives()[AST.getInterestedFile()]);
        return std::move(result);
    }

    auto buildForIndex(ASTInfo& AST) {
        TraverseTranslationUnitDecl(AST.tu());
        for(auto& [fid, directive]: AST.directives()) {
            collectDrectives(directive);
        }
        return std::move(indexResult);
    }

private:
    void addRange(clang::SourceRange range, FoldingRangeKind kind, std::string text) {
        /// In normal AST, the range must be valid. But unfortunately, the range
        /// may be invalid in incomplete AST, so we need to check it.
        if(range.isInvalid()) {
            return;
        }

        auto [begin, end] = range;
        begin = AST.getExpansionLoc(begin);
        end = AST.getExpansionLoc(end);

        /// If they are from the same macro expansion, skip it.
        if(begin == end) {
            return;
        }

        auto [fid, localRange] = AST.toLocalRange(clang::SourceRange(begin, end));
        auto [beginOffset, endOffset] = localRange;

        auto& ranges = interestedOnly ? result : indexResult[fid];
        ranges.emplace_back(localRange, kind, std::move(text));
    }

    void collectParameterList(clang::SourceLocation left, clang::SourceLocation right) {
        collectParameterList(clang::SourceRange(left, right));
    }

    /// Collect function parameter list between '(' and ')'.
    void collectParameterList(clang::SourceRange bounds) {
        auto tokens = AST.tokBuf().expandedTokens(bounds);
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

        addRange({leftParen.front().location(), rightParenIter->location()},
                 FoldingRangeKind::FunctionParams,
                 "(...)");
    }

    void collectCompoundStmt(const clang::Stmt* stmt) {
        if(auto* CS = llvm::dyn_cast<clang::CompoundStmt>(stmt)) {
            addRange({CS->getLBracLoc(), CS->getRBracLoc()},
                     FoldingRangeKind::CompoundStmt,
                     "{...}");
            for(auto child: stmt->children()) {
                collectCompoundStmt(child);
            }
        }
    }

    using ASTDirectives = std::remove_reference_t<decltype(std::declval<ASTInfo>().directives())>;

    void collectDrectives(const Directive& directive) {
        collectConditionMacro(directive.conditions);
        collectPragmaRegion(directive.pragmas);

        /// TODO:
        /// Collect multiline include statement.
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
                        addRange({last->conditionRange.getEnd(), cond.loc},
                                 FoldingRangeKind::ConditionDirective,
                                 "");
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
                            /// collect({last->conditionRange.getBegin(), cond.loc}, {0, -1});
                        } else {
                            /// collect({last->loc, cond.loc},
                            ///        {refl::enum_name(cond.kind).length(), -1});
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

        llvm::SmallVector<const Pragma*> stack;
        for(auto& pragma: pragmas) {
            if(pragma.kind == Pragma::Region) {
                stack.push_back(&pragma);
            } else if(pragma.kind == Pragma::EndRegion) {
                if(stack.empty()) {
                    continue;
                }

                auto last = stack.pop_back_val();
                addRange(clang::SourceRange(last->loc, pragma.loc), FoldingRangeKind::Region, "");
            }
        }
    }

private:
    clang::SourceManager& SM;
    clang::syntax::TokenBuffer& TB;
    std::vector<FoldingRange> result;
    index::Shared<std::vector<FoldingRange>> indexResult;
};

}  // namespace

std::vector<FoldingRange> foldingRange(ASTInfo& AST) {
    return FoldingRangeCollector(AST, true).buildForFile(AST);
}

index::Shared<std::vector<FoldingRange>> indexFoldingRange(ASTInfo& AST) {
    return FoldingRangeCollector(AST, false).buildForIndex(AST);

}  // namespace feature

}  // namespace clice::feature
